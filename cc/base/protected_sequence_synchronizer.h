// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_
#define CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_

namespace cc {

// ProtectedSequenceSynchronizer can be used to enforce thread safety of data
// that are owned and produced by an "owner" thread; and then passed by
// reference to another thread to use for a limited duration (a "protected
// sequence").
class ProtectedSequenceSynchronizer {
 public:
  ProtectedSequenceSynchronizer() = default;
  virtual ~ProtectedSequenceSynchronizer() = default;

  // Returns true if the current thread is the owner and producer of these data.
  virtual bool IsOwnerThread() const = 0;

  // Returns true if a non-owner thread is currently running a protected
  // sequence.
  virtual bool InProtectedSequence() const = 0;

  // Blocks execution of the owner thread until a non-owner thread finishes
  // executing a protected sequence.
  //
  // It is an error for this to be called on a non-owner thread.
  virtual void WaitForProtectedSequenceCompletion() const = 0;
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
  explicit ProtectedSequenceReadable(Args... args) : value_(args...) {}

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
  explicit ProtectedSequenceWritable(Args... args) : value_(args...) {}

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

}  // namespace cc

#endif  // CC_BASE_PROTECTED_SEQUENCE_SYNCHRONIZER_H_
