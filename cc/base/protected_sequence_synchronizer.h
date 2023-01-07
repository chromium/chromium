// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_
#define CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_

#include <memory>
#include <utility>

namespace cc {

// ProtectedSequenceSynchronizer can be used to enforce thread safety of data
// that are owned and produced by an "owner" thread; and then passed by
// reference to another thread to use for a limited duration (a "protected
// sequence"). A protected sequence must be initiated on the owning thread, and
// it must be concluded on the non-owning thread. See the code comment above
// InProtectedSequence() for more on this requirement.
class ProtectedSequenceSynchronizer {
 public:
  ProtectedSequenceSynchronizer() = default;
  ProtectedSequenceSynchronizer(const ProtectedSequenceSynchronizer&) = delete;
  ProtectedSequenceSynchronizer& operator=(
      const ProtectedSequenceSynchronizer&) = delete;
  virtual ~ProtectedSequenceSynchronizer() = default;

  // Returns true if the current thread is the owner and producer of these data.
  virtual bool IsOwnerThread() const = 0;

  // Returns true if a non-owner thread is currently running a protected
  // sequence. The owner thread must be responsible for transitioning the return
  // value from false to true, and the non-owner thread must be responsible for
  // transitioning from true to false. Failure to adhere to these guidelines
  // will likely cause race conditions and/or deadlock.
  virtual bool InProtectedSequence() const = 0;

  // Blocks execution of the owner thread until a non-owner thread finishes
  // executing a protected sequence. It is an error for this to be called on a
  // non-owner thread.
  virtual void WaitForProtectedSequenceCompletion() const = 0;
};

// ProtectedSequenceForbidden values cannot be accessed for read or write by any
// non-owner thread. There are no restrictions on access by the owner thread.
template <typename T>
class ProtectedSequenceForbidden {
 public:
  template <typename... Args>
  explicit ProtectedSequenceForbidden(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceForbidden(const ProtectedSequenceForbidden&) = delete;
  ProtectedSequenceForbidden& operator=(const ProtectedSequenceForbidden&) =
      delete;

  const T& Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    DCHECK(synchronizer.IsOwnerThread());
    return value_;
  }

  T& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread());
    return value_;
  }

 private:
  T value_;
};

// ProtectedSequenceReadable values are...
//   - readable by the owner thread at any time without blocking
//   - writable by the owner thread, but not during a protected sequence
//   - readable by a non-owner thread during a protected sequence
//   - never writable by a non-owner thread
template <typename T>
class ProtectedSequenceReadable {
 public:
  template <typename... Args>
  explicit ProtectedSequenceReadable(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceReadable(const ProtectedSequenceReadable&) = delete;
  ProtectedSequenceReadable& operator=(const ProtectedSequenceReadable&) =
      delete;

  const T& Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    DCHECK(synchronizer.IsOwnerThread() || synchronizer.InProtectedSequence());
    return value_;
  }

  T& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread());
    synchronizer.WaitForProtectedSequenceCompletion();
    return value_;
  }

 private:
  T value_;
};

// ProtectedSequenceWritable values are...
//   - readable by the owner thread, but not during a protected sequence
//   - writable by the owner thread, but not during a protected sequence
//   - readable by a non-owner thread during a protected sequence
//   - writable by a non-owner thread during a protected sequence
//
// Note that it is not safe to use ProtectedSequenceWritable values concurrently
// on two or more non-owner threads.
template <typename T>
class ProtectedSequenceWritable {
 public:
  template <typename... Args>
  explicit ProtectedSequenceWritable(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceWritable(const ProtectedSequenceWritable&) = delete;
  ProtectedSequenceWritable& operator=(const ProtectedSequenceWritable&) =
      delete;

  const T& Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    DCHECK(synchronizer.IsOwnerThread() || synchronizer.InProtectedSequence());
    if (synchronizer.IsOwnerThread())
      synchronizer.WaitForProtectedSequenceCompletion();
    return value_;
  }

  T& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread() || synchronizer.InProtectedSequence());
    if (synchronizer.IsOwnerThread())
      synchronizer.WaitForProtectedSequenceCompletion();
    return value_;
  }

 private:
  T value_;
};

// Type specializations for various containers.

template <typename T>
class ProtectedSequenceForbidden<std::unique_ptr<T>> {
 public:
  template <typename... Args>
  explicit ProtectedSequenceForbidden(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceForbidden(const ProtectedSequenceForbidden&) = delete;
  ProtectedSequenceForbidden& operator=(const ProtectedSequenceForbidden&) =
      delete;

  const T* Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    DCHECK(synchronizer.IsOwnerThread());
    return value_.get();
  }

  std::unique_ptr<T>& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread());
    return value_;
  }

 private:
  std::unique_ptr<T> value_;
};

template <typename T>
class ProtectedSequenceReadable<std::unique_ptr<T>> {
 public:
  template <typename... Args>
  explicit ProtectedSequenceReadable(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceReadable(const ProtectedSequenceReadable&) = delete;
  ProtectedSequenceReadable& operator=(const ProtectedSequenceReadable&) =
      delete;

  const T* Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    return value_.get();
  }

  std::unique_ptr<T>& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread());
    synchronizer.WaitForProtectedSequenceCompletion();
    return value_;
  }

 private:
  std::unique_ptr<T> value_;
};

template <typename T>
class ProtectedSequenceWritable<std::unique_ptr<T>> {
 public:
  template <typename... Args>
  explicit ProtectedSequenceWritable(Args&&... args)
      : value_(std::forward<Args>(args)...) {}

  ProtectedSequenceWritable(const ProtectedSequenceWritable&) = delete;
  ProtectedSequenceWritable& operator=(const ProtectedSequenceWritable&) =
      delete;

  const T* Read(const ProtectedSequenceSynchronizer& synchronizer) const {
    DCHECK(synchronizer.IsOwnerThread() || synchronizer.InProtectedSequence());
    if (synchronizer.IsOwnerThread())
      synchronizer.WaitForProtectedSequenceCompletion();
    return value_.get();
  }

  std::unique_ptr<T>& Write(const ProtectedSequenceSynchronizer& synchronizer) {
    DCHECK(synchronizer.IsOwnerThread() || synchronizer.InProtectedSequence());
    if (synchronizer.IsOwnerThread())
      synchronizer.WaitForProtectedSequenceCompletion();
    return value_;
  }

 private:
  std::unique_ptr<T> value_;
};

}  // namespace cc

#endif  // CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_
