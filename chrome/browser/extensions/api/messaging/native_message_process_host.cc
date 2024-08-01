// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/kill.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace {

// Maximum message size in bytes for messages received from Native Messaging
// hosts. Message size is limited mainly to prevent Chrome from crashing when
// native application misbehaves (e.g. starts writing garbage to the pipe).
const size_t kMaximumNativeMessageSize = 1024 * 1024;

// Message header contains 4-byte integer size of the message.
const size_t kMessageHeaderSize = 4;

// Size of the buffer to be allocated for each read.
const size_t kReadBufferSize = 4096;

base::FilePath GetProfilePathIfEnabled(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    const std::string& host_id) {
  return extensions::ExtensionSupportsConnectionFromNativeApp(
             extension_id, host_id, profile, /* log_errors = */ false)
             ? profile->GetPath()
             : base::FilePath();
}

}  // namespace

namespace extensions {

NativeMessageProcessHost::NativeMessageProcessHost(
    const ExtensionId& source_extension_id,
    const std::string& native_host_name,
    std::unique_ptr<NativeProcessLauncher> launcher)
    : client_(nullptr),
      source_extension_id_(source_extension_id),
      native_host_name_(native_host_name),
      launcher_(std::move(launcher)),
      closed_(false),
#if BUILDFLAG(IS_POSIX)
      read_file_(-1),
#endif
      read_pending_(false),
      write_pending_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_runner_ = content::GetIOThreadTaskRunner({});
}

NativeMessageProcessHost::~NativeMessageProcessHost() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (process_.IsValid()) {
// Kill the host process if necessary to make sure we don't leave zombies.
// TODO(crbug.com/41367359): On OSX EnsureProcessTerminated() may
// block, so we have to post a task on the blocking pool.
#if BUILDFLAG(IS_MAC)
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&base::EnsureProcessTerminated, std::move(process_)));
#else
    base::EnsureProcessTerminated(std::move(process_));
#endif
  }
}

// static
std::unique_ptr<NativeMessageHost> NativeMessageHost::Create(
    content::BrowserContext* browser_context,
    gfx::NativeView native_view,
    const ExtensionId& source_extension_id,
    const std::string& native_host_name,
    bool allow_user_level,
    std::string* error_message) {
  return NativeMessageProcessHost::CreateWithLauncher(
      source_extension_id, native_host_name,
      NativeProcessLauncher::CreateDefault(
          allow_user_level, native_view,
          GetProfilePathIfEnabled(Profile::FromBrowserContext(browser_context),
                                  source_extension_id, native_host_name),
          /* require_native_initiated_connections = */ false,
          /* connect_id = */ "", /* error_arg = */ "",
          Profile::FromBrowserContext(browser_context)));
}

// static
std::unique_ptr<NativeMessageHost> NativeMessageProcessHost::CreateWithLauncher(
    const ExtensionId& source_extension_id,
    const std::string& native_host_name,
    std::unique_ptr<NativeProcessLauncher> launcher) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<NativeMessageHost> process(new NativeMessageProcessHost(
      source_extension_id, native_host_name, std::move(launcher)));

  return process;
}

void NativeMessageProcessHost::LaunchHostProcess() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  GURL origin(std::string(kExtensionScheme) + "://" + source_extension_id_);
  launcher_->Launch(
      origin, native_host_name_,
      base::BindOnce(&NativeMessageProcessHost::OnHostProcessLaunched,
                     weak_factory_.GetWeakPtr()));
}

void NativeMessageProcessHost::OnHostProcessLaunched(
    NativeProcessLauncher::LaunchResult result,
    base::Process process,
    base::PlatformFile read_file,
    std::unique_ptr<net::FileStream> read_stream,
    std::unique_ptr<net::FileStream> write_stream) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (result) {
    case NativeProcessLauncher::RESULT_INVALID_NAME:
      Close(kInvalidNameError);
      return;
    case NativeProcessLauncher::RESULT_NOT_FOUND:
      Close(kNotFoundError);
      return;
    case NativeProcessLauncher::RESULT_FORBIDDEN:
      Close(kForbiddenError);
      return;
    case NativeProcessLauncher::RESULT_FAILED_TO_START:
      Close(kFailedToStartError);
      return;
    case NativeProcessLauncher::RESULT_SUCCESS:
      break;
  }

  process_ = std::move(process);
#if BUILDFLAG(IS_POSIX)
  // |read_stream| owns |read_file|, yet the underlying file descript is needed
  // for FileDescriptorWatcher.
  read_file_ = read_file;
#endif
  read_stream_ = std::move(read_stream);
  write_stream_ = std::move(write_stream);

  WaitRead();
  DoWrite();
}

void NativeMessageProcessHost::OnMessage(const std::string& json) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (closed_)
    return;

  // Allocate new buffer for the message.
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(json.size() +
                                                  kMessageHeaderSize);

  // Copy size and content of the message to the buffer.
  static_assert(sizeof(uint32_t) == kMessageHeaderSize,
                "kMessageHeaderSize is incorrect");
  *reinterpret_cast<uint32_t*>(buffer->data()) = json.size();
  memcpy(buffer->data() + kMessageHeaderSize, json.data(), json.size());

  // Push new message to the write queue.
  write_queue_.push(buffer);

  // Send() may be called before the host process is started. In that case the
  // message will be written when OnHostProcessLaunched() is called. If it's
  // already started then write the message now.
  if (write_stream_)
    DoWrite();
}

void NativeMessageProcessHost::Start(Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!client_);
  client_ = client;
  // It's safe to use base::Unretained() here because NativeMessagePort always
  // deletes us on the IO thread.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&NativeMessageProcessHost::LaunchHostProcess,
                                weak_factory_.GetWeakPtr()));
}

scoped_refptr<base::SingleThreadTaskRunner>
NativeMessageProcessHost::task_runner() const {
  return task_runner_;
}

void NativeMessageProcessHost::WaitRead() {
  if (closed_)
    return;

  DCHECK(!read_pending_);

  // On POSIX FileStream::Read() uses blocking thread pool, so it's better to
  // wait for the file to become readable before calling DoRead(). Otherwise it
  // would always be consuming one thread in the thread pool. On Windows
  // FileStream uses overlapped IO, so that optimization isn't necessary there.
#if BUILDFLAG(IS_POSIX)
  if (!read_controller_) {
    read_controller_ = base::FileDescriptorWatcher::WatchReadable(
        read_file_, base::BindRepeating(&NativeMessageProcessHost::DoRead,
                                        base::Unretained(this)));
  }
#else   // BUILDFLAG(IS_POSIX)
  DoRead();
#endif  // BUILDFLAG(IS_POSIX)
}

void NativeMessageProcessHost::DoRead() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  while (!closed_ && !read_pending_) {
    read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kReadBufferSize);
    int result =
        read_stream_->Read(read_buffer_.get(), kReadBufferSize,
                           base::BindOnce(&NativeMessageProcessHost::OnRead,
                                          weak_factory_.GetWeakPtr()));
    HandleReadResult(result);
  }
}

void NativeMessageProcessHost::OnRead(int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(read_pending_);
  read_pending_ = false;

  HandleReadResult(result);
  WaitRead();
}

void NativeMessageProcessHost::HandleReadResult(int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (closed_)
    return;

  if (result > 0) {
    ProcessIncomingData(read_buffer_->data(), result);
  } else if (result == net::ERR_IO_PENDING) {
    read_pending_ = true;
  } else if (result == 0 || result == net::ERR_CONNECTION_RESET) {
    // On Windows we get net::ERR_CONNECTION_RESET for a broken pipe, while on
    // Posix read() returns 0 in that case.
    Close(kNativeHostExited);
  } else {
    Close(kHostInputOutputError);
  }
}

void NativeMessageProcessHost::ProcessIncomingData(
    const char* data, int data_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  incoming_data_.append(data, data_size);

  while (true) {
    if (incoming_data_.size() < kMessageHeaderSize)
      return;

    size_t message_size =
        *reinterpret_cast<const uint32_t*>(incoming_data_.data());

    if (message_size > kMaximumNativeMessageSize) {
      LOG(ERROR) << "Native Messaging host tried sending a message that is "
                 << message_size << " bytes long.";
      Close(kHostInputOutputError);
      return;
    }

    if (incoming_data_.size() < message_size + kMessageHeaderSize)
      return;

    client_->PostMessageFromNativeHost(
        incoming_data_.substr(kMessageHeaderSize, message_size));

    incoming_data_.erase(0, kMessageHeaderSize + message_size);
  }
}

void NativeMessageProcessHost::DoWrite() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  while (!write_pending_ && !closed_) {
    if (!current_write_buffer_.get() ||
        !current_write_buffer_->BytesRemaining()) {
      if (write_queue_.empty())
        return;
      scoped_refptr<net::IOBufferWithSize> buffer =
          std::move(write_queue_.front());
      int buffer_size = buffer->size();
      current_write_buffer_ = base::MakeRefCounted<net::DrainableIOBuffer>(
          std::move(buffer), buffer_size);
      write_queue_.pop();
    }

    int result = write_stream_->Write(
        current_write_buffer_.get(), current_write_buffer_->BytesRemaining(),
        base::BindOnce(&NativeMessageProcessHost::OnWritten,
                       weak_factory_.GetWeakPtr()));
    HandleWriteResult(result);
  }
}

void NativeMessageProcessHost::HandleWriteResult(int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (result <= 0) {
    if (result == net::ERR_IO_PENDING) {
      write_pending_ = true;
    } else {
      LOG(ERROR) << "Error when writing to Native Messaging host: " << result;
      Close(kHostInputOutputError);
    }
    return;
  }

  current_write_buffer_->DidConsume(result);
}

void NativeMessageProcessHost::OnWritten(int result) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  DCHECK(write_pending_);
  write_pending_ = false;

  HandleWriteResult(result);
  DoWrite();
}

void NativeMessageProcessHost::Close(const std::string& error_message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!closed_) {
    closed_ = true;
#if BUILDFLAG(IS_POSIX)
    read_controller_.reset();
#endif
    read_stream_.reset();
    write_stream_.reset();
    client_->CloseChannel(error_message);
  }
}

}  // namespace extensions
