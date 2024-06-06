// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_PERSISTENT_PROTO_H_
#define ASH_UTILITY_PERSISTENT_PROTO_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"

namespace ash {

namespace internal {

// Data types ------------------------------------------------------------------

// The result of reading a backing file from disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class ReadStatus {
  kOk = 0,
  kMissing = 1,
  kReadError = 2,
  kParseError = 3,
  // kNoop is currently unused, but was previously used when no read was
  // required.
  kNoop = 4,
  kMaxValue = kNoop,
};

// The result of writing a backing file to disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class WriteStatus {
  kOk = 0,
  kWriteError = 1,
  kSerializationError = 2,
  kMaxValue = kSerializationError,
};

// Helpers ---------------------------------------------------------------------

template <class T>
std::pair<ReadStatus, std::unique_ptr<T>> Read(const base::FilePath& filepath) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(filepath))
    return {ReadStatus::kMissing, nullptr};

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str))
    return {ReadStatus::kReadError, nullptr};

  auto proto = std::make_unique<T>();
  if (!proto->ParseFromString(proto_str))
    return {ReadStatus::kParseError, nullptr};

  return {ReadStatus::kOk, std::move(proto)};
}

// Writes `proto_str` to the file specified by `filepath`.
WriteStatus ASH_EXPORT Write(const base::FilePath& filepath,
                             std::string_view proto_str);

}  // namespace internal

// PersistentProto wraps a proto class and persists it to disk. Usage summary:
// 1. Init is asynchronous. Using the object before initialization is complete
//     will result in a crash.
// 2. pproto->Method() will call Method on the underlying proto.
// 3. Call `QueueWrite()` to write to disk.
//
// Reading. The backing file is loaded asynchronously during initialization.
// Until initialization completed, `has_value()` will be false and `get()` will
// return `nullptr`. Register an init callback to be notified of completion.
//
// Writing. Writes must be triggered manually. Two methods are available:
// 1. `QueueWrite()` delays writing to disk for `write_delay` time, in order to
//    batch successive writes.
// 2. `StartWrite()` writes to disk as soon as the task scheduler allows.
// Registered write callbacks are executed whenever a write operation finishes.
template <class T>
class ASH_EXPORT PersistentProto {
 public:
  using InitCallback = base::OnceClosure;
  using WriteCallback = base::RepeatingCallback<void(/*success=*/bool)>;

  PersistentProto(
      const base::FilePath& path,
      const base::TimeDelta write_delay,
      base::TaskPriority task_priority = base::TaskPriority::BEST_EFFORT)
      : path_(path),
        write_delay_(write_delay),
        on_init_callbacks_(
            std::make_unique<base::OnceCallbackList<InitCallback::RunType>>()),
        on_write_callbacks_(
            std::make_unique<
                base::RepeatingCallbackList<WriteCallback::RunType>>()),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {task_priority, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  ~PersistentProto() = default;

  PersistentProto(const PersistentProto&) = delete;
  PersistentProto& operator=(const PersistentProto&) = delete;

  PersistentProto(PersistentProto&& other) {
    path_ = other.path_;
    write_delay_ = other.write_delay_;
    initialized_ = other.initialized_;
    write_is_queued_ = false;
    purge_after_reading_ = other.purge_after_reading_;
    on_init_callbacks_ = std::move(other.on_init_callbacks_);
    on_write_callbacks_ = std::move(other.on_write_callbacks_);
    task_runner_ = std::move(other.task_runner_);
    proto_ = std::move(other.proto_);
  }

  void Init() {
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&internal::Read<T>, path_),
        base::BindOnce(&PersistentProto<T>::OnReadComplete,
                       weak_factory_.GetWeakPtr()));
  }

  [[nodiscard]] base::CallbackListSubscription RegisterOnInit(
      InitCallback on_init) {
    return on_init_callbacks_->Add(std::move(on_init));
  }

  // NOTE: The caller must ensure `on_init` to be valid when init completes.
  void RegisterOnInitUnsafe(InitCallback on_init) {
    on_init_callbacks_->AddUnsafe(std::move(on_init));
  }

  // NOTE: The caller must ensure `on_write` to be valid during the life cycle
  // of `PersistentProto`.
  void RegisterOnWriteUnsafe(WriteCallback on_write) {
    on_write_callbacks_->AddUnsafe(std::move(on_write));
  }

  T* get() { return proto_.get(); }

  T* operator->() {
    CHECK(proto_);
    return proto_.get();
  }

  const T* operator->() const {
    CHECK(proto_);
    return proto_.get();
  }

  T operator*() {
    CHECK(proto_);
    return *proto_;
  }

  bool initialized() const { return initialized_; }

  constexpr bool has_value() const { return proto_.get() != nullptr; }

  constexpr explicit operator bool() const { return has_value(); }

  // Write the backing proto to disk after `save_delay_ms_` has elapsed.
  void QueueWrite() {
    DCHECK(proto_);
    if (!proto_)
      return;

    // If a save is already queued, do nothing.
    if (write_is_queued_)
      return;
    write_is_queued_ = true;

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PersistentProto<T>::OnQueueWrite,
                       weak_factory_.GetWeakPtr()),
        write_delay_);
  }

  // Write the backing proto to disk 'now'.
  void StartWrite() {
    DCHECK(proto_);
    if (!proto_)
      return;

    // Serialize the proto outside of the posted task, because otherwise we need
    // to pass a proto pointer into the task. This causes a rare race condition
    // during destruction where the proto can be destroyed before serialization,
    // causing a crash.
    std::string proto_str;
    if (!proto_->SerializeToString(&proto_str))
      OnWriteComplete(internal::WriteStatus::kSerializationError);

    // The SequentialTaskRunner ensures the writes won't trip over each other,
    // so we can schedule without checking whether another write is currently
    // active.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&internal::Write, path_, proto_str),
        base::BindOnce(&PersistentProto<T>::OnWriteComplete,
                       weak_factory_.GetWeakPtr()));
  }

  // Safely clear this proto from memory and disk. This is preferred to clearing
  // the proto, because it ensures the proto is purged even if called before the
  // backing file is read from disk. In this case, the file is overwritten after
  // it has been read. In either case, the file is written as soon as possible,
  // skipping the `save_delay_ms_` wait time.
  void Purge() {
    if (proto_) {
      proto_.reset();
      proto_ = std::make_unique<T>();
      StartWrite();
    } else {
      purge_after_reading_ = true;
    }
  }

 private:
  void OnReadComplete(
      std::pair<internal::ReadStatus, std::unique_ptr<T>> result) {
    const internal::ReadStatus status = result.first;
    base::UmaHistogramEnumeration("Apps.AppList.PersistentProto.ReadStatus",
                                  status);

    if (status == internal::ReadStatus::kOk) {
      proto_ = std::move(result.second);
    } else {
      proto_ = std::make_unique<T>();
      QueueWrite();
    }

    if (purge_after_reading_) {
      proto_.reset();
      proto_ = std::make_unique<T>();
      StartWrite();
      purge_after_reading_ = false;
    }

    initialized_ = true;
    on_init_callbacks_->Notify();
  }

  void OnWriteComplete(const internal::WriteStatus status) {
    base::UmaHistogramEnumeration("Apps.AppList.PersistentProto.WriteStatus",
                                  status);
    on_write_callbacks_->Notify(/*success=*/status ==
                                internal::WriteStatus::kOk);
  }

  void OnQueueWrite() {
    // Reset the queued flag before posting the task. Last-moment updates to
    // `proto_` will post another task to write the proto, avoiding race
    // conditions.
    write_is_queued_ = false;
    StartWrite();
  }

  // Path on disk to read from and write to.
  base::FilePath path_;

  // How long to delay writing to disk for on a call to QueueWrite.
  base::TimeDelta write_delay_;

  // Whether the proto has finished reading from disk. `proto_` will be empty
  // before `initialized_` is true.
  bool initialized_ = false;

  // Whether or not a write is currently scheduled.
  bool write_is_queued_ = false;

  // Whether we should immediately clear the proto after reading it.
  bool purge_after_reading_ = false;

  // Run when `proto_` finishes initialization.
  std::unique_ptr<base::OnceCallbackList<InitCallback::RunType>>
      on_init_callbacks_;

  // Run when the cache finishes writing to disk.
  std::unique_ptr<base::RepeatingCallbackList<WriteCallback::RunType>>
      on_write_callbacks_;

  // The proto itself.
  std::unique_ptr<T> proto_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<PersistentProto> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_UTILITY_PERSISTENT_PROTO_H_
