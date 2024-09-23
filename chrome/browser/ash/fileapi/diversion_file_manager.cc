// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/fileapi/diversion_file_manager.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/clamped_math.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/buildflag.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "storage/common/file_system/file_system_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include <fcntl.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#else
#error "DiversionFileManager code only builds on ChromeOS"
#endif

namespace ash {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// In theory, we could get this at runtime via Profile::GetPath and
// ProfileManager::GetActiveUserProfile, but calling those needs to happen on
// the UI thread while the code in this C++ primarily happens on the IO thread.
// It's simpler, assuming we're on ChromeOS, to just hard-code the home dir.
constexpr char kChronosHomeDir[] = "/home/chronos/user/";
#else
#error "DiversionFileManager code only builds on ChromeOS"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using GetFileInfoCallback =
    base::OnceCallback<void(base::File::Error result,
                            const base::File::Info& file_info)>;
using StatusCallback = base::OnceCallback<void(base::File::Error result)>;

// A temporary file (as a file descriptor) and its size in bytes, or the first
// error (if any) encountered in accessing it.
//
// The underlying file (in the kernel sense) is an O_TMPFILE file (and so does
// not need any further Chromium code for garbage collecting used files),
// created under kChronosHomeDir (not /tmp) so that it is disk-backed instead
// of memory-backed, as disk is more plentiful than RAM.
//
// O_TMPFILE and the hard-coded kChronosHomeDir may be Linux-only and
// ChromeOS-only concepts but this C++ file only builds when IS_CHROMEOS_ASH.
struct Tmpfile {
  base::ScopedFD scoped_fd;
  int64_t file_size = 0;
  net::Error net_error = net::OK;

  Tmpfile() = default;
  explicit Tmpfile(net::Error e) : net_error(e) {}
};

// An asynchronous operation involving a transform function that runs on a
// base::BlockingType::MAY_BLOCK thread - it can perform blocking I/O. A
// Tmpfile (which holds a base::ScopedFD, an ownership type) is passed to that
// thread and is returned to the original (content::BrowserThread::IO) thread
// via the Callback.
//
// The transform field may be a null OnceCallback, meaning the identity (no-op)
// transform. The Callback field must not be a null OnceCallback.
struct Op {
  // The int means whatever the Transform and Callback agree that it means. It
  // often means the number of bytes read or written, or it is ignored. The
  // Tmpfile itself already holds a net::Error.
  using Transform = base::OnceCallback<std::pair<Tmpfile, int>(Tmpfile)>;
  using Callback = base::OnceCallback<void(const Tmpfile&, int)>;

  Transform transform;
  Callback callback;
};

}  // namespace

// A DiversionFileManager is essentially a map from FileSystemURL to
// scoped_refptr<Entry>. An Entry serializes the Ops enqueued to it such that
// at most one Op is in-flight at a time. It also holds the Tmpfile (which owns
// the underlying file descriptor, via a ScopedFD) when that Tmpfile is not
// loaned out to any in-flight Op.
//
// An Entry also starts an idle_timeout timer (1) at construction and (2) after
// each time the last (so far) reader/writer Worker is destroyed. If no new
// Workers were created (and Finish was not called) between a timer starting
// and expiring, the Entry times out and its implicit_callback is run.
class DiversionFileManager::Entry
    : public base::RefCounted<DiversionFileManager::Entry> {
 public:
  Entry(const storage::FileSystemURL& url,
        scoped_refptr<DiversionFileManager> manager,
        base::TimeDelta idle_timeout,
        Callback implicit_callback);

  void OnWorkerConstructed();
  void OnWorkerDestroyed();

  void Enqueue(Op op);

  void Finish(Callback explicit_callback);

 private:
  friend class base::RefCounted<DiversionFileManager::Entry>;
  ~Entry();

  void Run(Op op);
  void OnRunComplete(Op::Callback callback, std::pair<Tmpfile, int> tmpfile);
  bool ShouldRunCallback();
  void RunCallback();
  bool ShouldPostIdleTimer();
  void PostIdleTimer();

  static void OnIdleTimer(
      scoped_refptr<Entry> refptr_this,
      uint64_t num_workers_constructed_as_at_post_idle_timer);

  const storage::FileSystemURL url_;
  scoped_refptr<DiversionFileManager> manager_;
  base::TimeDelta idle_timeout_;

  Tmpfile tmpfile_;
  uint64_t num_workers_constructed_ = 0;
  uint64_t num_workers_destroyed_ = 0;
  std::optional<StoppedReason> stopped_reason_ = std::nullopt;
  bool is_running_an_op_ = false;
  base::circular_deque<Op> pending_ops_;
  Callback implicit_callback_;
  Callback explicit_callback_;
};

DiversionFileManager::Entry::Entry(const storage::FileSystemURL& url,
                                   scoped_refptr<DiversionFileManager> manager,
                                   base::TimeDelta idle_timeout,
                                   Callback implicit_callback)
    : url_(url),
      manager_(manager),
      idle_timeout_(idle_timeout),
      implicit_callback_(std::move(implicit_callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // transform creates the O_TMPFILE file.
  static constexpr auto transform =
      [](std::string tmpfile_dir, Tmpfile tmpfile) -> std::pair<Tmpfile, int> {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::ScopedFD sfd(open(tmpfile_dir.c_str(),
                            O_CLOEXEC | O_EXCL | O_TMPFILE | O_RDWR, 0600));
    if (sfd.is_valid()) {
      tmpfile.scoped_fd = std::move(sfd);
    } else {
      tmpfile.net_error = net::ERR_FILE_NO_SPACE;
    }
    return std::make_pair(std::move(tmpfile), 0);
  };

  Run({base::BindOnce(transform, manager->TmpfileDirAsString()),
       Op::Callback()});
  PostIdleTimer();
}

DiversionFileManager::Entry::~Entry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void DiversionFileManager::Entry::OnWorkerConstructed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  num_workers_constructed_++;
}

void DiversionFileManager::Entry::OnWorkerDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  num_workers_destroyed_++;
  if (ShouldRunCallback()) {
    RunCallback();
  }
  if (ShouldPostIdleTimer()) {
    PostIdleTimer();
  }
}

void DiversionFileManager::Entry::Enqueue(Op op) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (is_running_an_op_) {
    pending_ops_.push_back(std::move(op));
  } else {
    Run(std::move(op));
  }
}

void DiversionFileManager::Entry::Run(Op op) {
  CHECK(!is_running_an_op_);
  is_running_an_op_ = true;

  if (op.transform) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(std::move(op.transform), std::move(tmpfile_)),
        base::BindOnce(&Entry::OnRunComplete, scoped_refptr<Entry>(this),
                       std::move(op.callback)));
  } else {
    OnRunComplete(std::move(op.callback),
                  std::make_pair(std::move(tmpfile_), 0));
  }
}

void DiversionFileManager::Entry::OnRunComplete(
    Op::Callback op_callback,
    std::pair<Tmpfile, int> result) {
  CHECK(is_running_an_op_);
  is_running_an_op_ = false;

  tmpfile_ = std::move(result.first);

  if (op_callback) {
    std::move(op_callback).Run(tmpfile_, result.second);
  }

  if (!pending_ops_.empty()) {
    Op op = std::move(pending_ops_.front());
    pending_ops_.pop_front();
    Run(std::move(op));
  } else if (ShouldRunCallback()) {
    RunCallback();
  }
}

void DiversionFileManager::Entry::Finish(Callback explicit_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  CHECK(!stopped_reason_.has_value());
  stopped_reason_ = StoppedReason::kExplicitFinish;
  explicit_callback_ = std::move(explicit_callback);
  if (ShouldRunCallback()) {
    RunCallback();
  }
}

bool DiversionFileManager::Entry::ShouldRunCallback() {
  return !is_running_an_op_ && pending_ops_.empty() &&
         stopped_reason_.has_value() &&
         (num_workers_constructed_ == num_workers_destroyed_);
}

// Precondition: ShouldRunCallback().
void DiversionFileManager::Entry::RunCallback() {
  Callback callback;
  switch (stopped_reason_.value()) {
    case StoppedReason::kExplicitFinish:
      callback = std::move(explicit_callback_);
      break;
    case StoppedReason::kImplicitIdle:
      callback = std::move(implicit_callback_);
      break;
  }
  if (callback) {
    std::move(callback).Run(stopped_reason_.value(), url_,
                            std::move(tmpfile_.scoped_fd), tmpfile_.file_size,
                            storage::NetErrorToFileError(tmpfile_.net_error));
  }
}

bool DiversionFileManager::Entry::ShouldPostIdleTimer() {
  return !stopped_reason_.has_value() &&
         (num_workers_constructed_ == num_workers_destroyed_);
}

// Precondition: ShouldPostIdleTimer().
void DiversionFileManager::Entry::PostIdleTimer() {
  content::GetIOThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DiversionFileManager::Entry::OnIdleTimer,
                     scoped_refptr<Entry>(this), num_workers_constructed_),
      idle_timeout_);
}

// static
void DiversionFileManager::Entry::OnIdleTimer(
    scoped_refptr<Entry> refptr_this,
    uint64_t num_workers_constructed_as_at_post_idle_timer) {
  if (num_workers_constructed_as_at_post_idle_timer !=
      refptr_this->num_workers_constructed_) {
    return;
  } else if (refptr_this->stopped_reason_.has_value()) {
    return;
  }
  refptr_this->stopped_reason_ = StoppedReason::kImplicitIdle;

  DiversionFileManager::Map& map = refptr_this->manager_->entries_;
  if (auto iter = map.find(refptr_this->url_); iter != map.end()) {
    CHECK_EQ(refptr_this, iter->second);
    map.erase(iter);
  }

  if (refptr_this->ShouldRunCallback()) {
    refptr_this->RunCallback();
  }
}

// ----

// A reader or writer for an Entry's temporary-file. Each worker has an
// independent file-offset.
class DiversionFileManager::Worker : public storage::FileStreamReader,
                                     public storage::FileStreamWriter {
 public:
  enum class Role {
    kReader,
    kWriter,
  };

  Worker(Role role, scoped_refptr<Entry> entry, int64_t offset);
  ~Worker() override;

  // storage::FileStreamReader overrides.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

  // storage::FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(storage::FlushMode flush_mode,
            net::CompletionOnceCallback callback) override;

 private:
  void ReadOrWrite(net::IOBuffer* buf,
                   int buf_len,
                   net::CompletionOnceCallback callback);
  static void OnReadOrWrite(base::WeakPtr<Worker> weak_ptr,
                            net::CompletionOnceCallback callback,
                            const Tmpfile& tmpfile,
                            int byte_count);

  Role role_;
  scoped_refptr<Entry> entry_;
  int64_t offset_;

  base::WeakPtrFactory<Worker> weak_ptr_factory_{this};
};

DiversionFileManager::Worker::Worker(Role role,
                                     scoped_refptr<Entry> entry,
                                     int64_t offset)
    : role_(role), entry_(std::move(entry)), offset_(offset) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  entry_->OnWorkerConstructed();
}

DiversionFileManager::Worker::~Worker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  entry_->OnWorkerDestroyed();
}

int DiversionFileManager::Worker::Read(net::IOBuffer* buf,
                                       int buf_len,
                                       net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_EQ(role_, Role::kReader);
  ReadOrWrite(buf, buf_len, std::move(callback));
  return net::ERR_IO_PENDING;
}

int64_t DiversionFileManager::Worker::GetLength(
    net::Int64CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_EQ(role_, Role::kReader);

  static constexpr auto reply = [](net::Int64CompletionOnceCallback callback,
                                   const Tmpfile& tmpfile, int ignored) {
    std::move(callback).Run((tmpfile.net_error < 0)
                                ? static_cast<int64_t>(tmpfile.net_error)
                                : tmpfile.file_size);
  };

  entry_->Enqueue(
      {Op::Transform(), base::BindOnce(reply, std::move(callback))});

  return net::ERR_IO_PENDING;
}

int DiversionFileManager::Worker::Write(net::IOBuffer* buf,
                                        int buf_len,
                                        net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_EQ(role_, Role::kWriter);
  ReadOrWrite(buf, buf_len, std::move(callback));
  return net::ERR_IO_PENDING;
}

int DiversionFileManager::Worker::Cancel(net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_EQ(role_, Role::kWriter);
  // Unimplemented.
  return net::OK;
}

int DiversionFileManager::Worker::Flush(storage::FlushMode flush_mode,
                                        net::CompletionOnceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  CHECK_EQ(role_, Role::kWriter);
  // Flush is a no-op. An O_TMPFILE file isn't persistent anyway if the process
  // crashes. Within this process (and its open file descriptor), any
  // previously written bytes should already be readable.
  return net::OK;
}

void DiversionFileManager::Worker::ReadOrWrite(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback) {
  static constexpr auto transform =
      [](Role role, char* data_ptr, int data_len, int64_t offset,
         Tmpfile tmpfile) -> std::pair<Tmpfile, int> {
    if (tmpfile.net_error != net::OK) {
      return std::make_pair(std::move(tmpfile), 0);
    } else if (!tmpfile.scoped_fd.is_valid()) {
      return std::make_pair(Tmpfile(net::ERR_INVALID_HANDLE), 0);
    }

    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    const int64_t original_offset = offset;
    const int fd = tmpfile.scoped_fd.get();
    while (data_len > 0) {
      if (offset > std::numeric_limits<off_t>::max()) {
        return std::make_pair(Tmpfile(net::ERR_FILE_TOO_BIG), 0);
      }

      size_t arg2 = static_cast<size_t>(data_len);
      off_t arg3 = static_cast<off_t>(offset);
      ssize_t n = (role == Role::kReader)
                      ? HANDLE_EINTR(pread(fd, data_ptr, arg2, arg3))
                      : HANDLE_EINTR(pwrite(fd, data_ptr, arg2, arg3));

      if (n == 0) {
        break;
      } else if (n < 0) {
        return std::make_pair(Tmpfile((errno == ENOSPC) ? net::ERR_FILE_NO_SPACE
                                                        : net::ERR_FAILED),
                              0);
      }

      data_ptr += n;
      data_len -= static_cast<int>(n);
      offset = base::ClampAdd(offset, static_cast<int64_t>(n));
    }

    tmpfile.file_size = std::max(tmpfile.file_size, offset);
    return std::make_pair(std::move(tmpfile),
                          static_cast<int>(offset - original_offset));
  };

  entry_->Enqueue(
      {base::BindOnce(transform, role_, buf->data(), buf_len, offset_),
       base::BindOnce(&DiversionFileManager::Worker::OnReadOrWrite,
                      weak_ptr_factory_.GetWeakPtr(), std::move(callback))});
}

// static
void DiversionFileManager::Worker::OnReadOrWrite(
    base::WeakPtr<Worker> weak_ptr,
    net::CompletionOnceCallback callback,
    const Tmpfile& tmpfile,
    int byte_count) {
  Worker* weak_this = weak_ptr.get();
  if (weak_this && (byte_count > 0)) {
    weak_this->offset_ =
        base::ClampAdd(weak_this->offset_, static_cast<int64_t>(byte_count));
  }
  std::move(callback).Run((tmpfile.net_error < 0) ? tmpfile.net_error
                                                  : byte_count);
}

// ----

DiversionFileManager::DiversionFileManager() = default;

DiversionFileManager::~DiversionFileManager() = default;

bool DiversionFileManager::IsDiverting(const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  return entries_.find(url) != entries_.end();
}

DiversionFileManager::StartDivertingResult DiversionFileManager::StartDiverting(
    const storage::FileSystemURL& url,
    base::TimeDelta idle_timeout,
    Callback implicit_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const auto [iter, success] = entries_.insert(
      std::make_pair(url, base::MakeRefCounted<Entry>(
                              url, scoped_refptr<DiversionFileManager>(this),
                              idle_timeout, std::move(implicit_callback))));

  return success
             ? DiversionFileManager::StartDivertingResult::kOK
             : DiversionFileManager::StartDivertingResult::kWasAlreadyDiverted;
}

DiversionFileManager::FinishDivertingResult
DiversionFileManager::FinishDiverting(const storage::FileSystemURL& url,
                                      Callback explicit_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (auto iter = entries_.find(url); iter != entries_.end()) {
    scoped_refptr<Entry> entry = std::move(iter->second);
    entries_.erase(iter);
    entry->Finish(std::move(explicit_callback));
    return DiversionFileManager::FinishDivertingResult::kOK;
  }
  return DiversionFileManager::FinishDivertingResult::kWasNotDiverting;
}

std::unique_ptr<storage::FileStreamReader>
DiversionFileManager::CreateDivertedFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (auto iter = entries_.find(url); iter != entries_.end()) {
    return std::make_unique<Worker>(Worker::Role::kReader, iter->second,
                                    offset);
  }
  return nullptr;
}

std::unique_ptr<storage::FileStreamWriter>
DiversionFileManager::CreateDivertedFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (auto iter = entries_.find(url); iter != entries_.end()) {
    return std::make_unique<Worker>(Worker::Role::kWriter, iter->second,
                                    offset);
  }
  return nullptr;
}

void DiversionFileManager::GetDivertedFileInfo(
    const storage::FileSystemURL& url,
    storage::FileSystemOperation::GetMetadataFieldSet fields,
    GetFileInfoCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  static constexpr auto reply = [](GetFileInfoCallback callback,
                                   const Tmpfile& tmpfile, int ignored) {
    base::File::Info info;
    if (tmpfile.net_error != net::OK) {
      std::move(callback).Run(storage::NetErrorToFileError(tmpfile.net_error),
                              info);
      return;
    }
    info.size = tmpfile.file_size;
    info.is_directory = false;
    info.is_symbolic_link = false;
    std::move(callback).Run(base::File::FILE_OK, info);
  };

  if (auto iter = entries_.find(url); iter != entries_.end()) {
    iter->second->Enqueue(
        {Op::Transform(), base::BindOnce(reply, std::move(callback))});
    return;
  }

  base::File::Info info;
  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION, info);
}

void DiversionFileManager::TruncateDivertedFile(
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  static constexpr auto transform =
      [](int64_t length, Tmpfile tmpfile) -> std::pair<Tmpfile, int> {
    if (tmpfile.net_error != net::OK) {
      return std::make_pair(std::move(tmpfile), 0);
    } else if (!tmpfile.scoped_fd.is_valid()) {
      return std::make_pair(Tmpfile(net::ERR_INVALID_HANDLE), 0);
    } else if ((length < 0) || (static_cast<uint64_t>(length) >
                                std::numeric_limits<off_t>::max())) {
      return std::make_pair(Tmpfile(net::ERR_INVALID_ARGUMENT), 0);
    }

    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    if (HANDLE_EINTR(ftruncate(tmpfile.scoped_fd.get(),
                               static_cast<off_t>(length))) < 0) {
      tmpfile.net_error = net::ERR_FAILED;
    }

    return std::make_pair(std::move(tmpfile), 0);
  };

  static constexpr auto reply = [](StatusCallback callback,
                                   const Tmpfile& tmpfile, int ignored) {
    std::move(callback).Run(storage::NetErrorToFileError(tmpfile.net_error));
  };

  if (auto iter = entries_.find(url); iter != entries_.end()) {
    iter->second->Enqueue({base::BindOnce(transform, length),
                           base::BindOnce(reply, std::move(callback))});
    return;
  }

  std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void DiversionFileManager::OverrideTmpfileDirForTesting(
    const base::FilePath& tmpfile_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  tmpfile_dir_ = tmpfile_dir;
}

std::string DiversionFileManager::TmpfileDirAsString() const {
  return tmpfile_dir_.empty() ? std::string(kChronosHomeDir)
                              : tmpfile_dir_.AsUTF8Unsafe();
}

}  // namespace ash
