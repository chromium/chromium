// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_SCOPED_BLOCKING_CALL_H
#define BASE_THREADING_SCOPED_BLOCKING_CALL_H

#include "base/base_export.h"
#include "base/location.h"
#include "base/logging.h"

namespace base {

// A "blocking call" refers to any call that causes the calling thread to wait
// off-CPU. It includes but is not limited to calls that wait on synchronous
// file I/O operations: read or write a file from disk, interact with a pipe or
// a socket, rename or delete a file, enumerate files in a directory, etc.
// Acquiring a low contention lock is not considered a blocking call.

// BlockingType indicates the likelihood that a blocking call will actually
// block.
enum class BlockingType {
  // The call might block (e.g. file I/O that might hit in memory cache).
  MAY_BLOCK,
  // The call will definitely block (e.g. cache already checked and now pinging
  // server synchronously).
  WILL_BLOCK
};

namespace internal {

class BlockingObserver;

// Common implementation class for both ScopedBlockingCall and
// ScopedBlockingCallWithBaseSyncPrimitives without assertions.
class BASE_EXPORT UncheckedScopedBlockingCall {
 public:
  explicit UncheckedScopedBlockingCall(BlockingType blocking_type);
  ~UncheckedScopedBlockingCall();

 private:
  internal::BlockingObserver* const blocking_observer_;

  // Previous ScopedBlockingCall instantiated on this thread.
  UncheckedScopedBlockingCall* const previous_scoped_blocking_call_;

  // Whether the BlockingType of the current thread was WILL_BLOCK after this
  // ScopedBlockingCall was instantiated.
  const bool is_will_block_;

  DISALLOW_COPY_AND_ASSIGN(UncheckedScopedBlockingCall);
};

}  // namespace internal

// This class must be instantiated in every scope where a blocking call is made
// and serves as a precise annotation of the scope that may/will block for the
// scheduler. When a ScopedBlockingCall is instantiated, it asserts that
// blocking calls are allowed in its scope with a call to
// base::AssertBlockingAllowed(). CPU usage should be minimal within that scope.
// //base APIs that block instantiate their own ScopedBlockingCall; it is not
// necessary to instantiate another ScopedBlockingCall in the scope where these
// APIs are used. Nested ScopedBlockingCalls are supported (mostly a no-op
// except for WILL_BLOCK nested within MAY_BLOCK which will result in immediate
// WILL_BLOCK semantics).
//
// Good:
//   Data data;
//   {
//     ScopedBlockingCall scoped_blocking_call(
//         FROM_HERE, BlockingType::WILL_BLOCK);
//     data = GetDataFromNetwork();
//   }
//   CPUIntensiveProcessing(data);
//
// Bad:
//   ScopedBlockingCall scoped_blocking_call(FROM_HERE,
//       BlockingType::WILL_BLOCK);
//   Data data = GetDataFromNetwork();
//   CPUIntensiveProcessing(data);  // CPU usage within a ScopedBlockingCall.
//
// Good:
//   Data a;
//   Data b;
//   {
//     ScopedBlockingCall scoped_blocking_call(
//         FROM_HERE, BlockingType::MAY_BLOCK);
//     a = GetDataFromMemoryCacheOrNetwork();
//     b = GetDataFromMemoryCacheOrNetwork();
//   }
//   CPUIntensiveProcessing(a);
//   CPUIntensiveProcessing(b);
//
// Bad:
//   ScopedBlockingCall scoped_blocking_call(
//       FROM_HERE, BlockingType::MAY_BLOCK);
//   Data a = GetDataFromMemoryCacheOrNetwork();
//   Data b = GetDataFromMemoryCacheOrNetwork();
//   CPUIntensiveProcessing(a);  // CPU usage within a ScopedBlockingCall.
//   CPUIntensiveProcessing(b);  // CPU usage within a ScopedBlockingCall.
//
// Good:
//   base::WaitableEvent waitable_event(...);
//   waitable_event.Wait();
//
// Bad:
//  base::WaitableEvent waitable_event(...);
//  ScopedBlockingCall scoped_blocking_call(
//      FROM_HERE, BlockingType::WILL_BLOCK);
//  waitable_event.Wait();  // Wait() instantiates its own ScopedBlockingCall.
//
// When a ScopedBlockingCall is instantiated from a ThreadPool parallel or
// sequenced task, the thread pool size is incremented to compensate for the
// blocked thread (more or less aggressively depending on BlockingType).
class BASE_EXPORT ScopedBlockingCall
    : public internal::UncheckedScopedBlockingCall {
 public:
  ScopedBlockingCall(const Location& from_here, BlockingType blocking_type);
  ~ScopedBlockingCall();
};

namespace internal {

// This class must be instantiated in every scope where a sync primitive is
// used. When a ScopedBlockingCallWithBaseSyncPrimitives is instantiated, it
// asserts that sync primitives are allowed in its scope with a call to
// internal::AssertBaseSyncPrimitivesAllowed(). The same guidelines as for
// ScopedBlockingCall should be followed.
class BASE_EXPORT ScopedBlockingCallWithBaseSyncPrimitives
    : public UncheckedScopedBlockingCall {
 public:
  explicit ScopedBlockingCallWithBaseSyncPrimitives(BlockingType blocking_type);
  ScopedBlockingCallWithBaseSyncPrimitives(const Location& from_here,
                                           BlockingType blocking_type);
  ~ScopedBlockingCallWithBaseSyncPrimitives();
};

// Interface for an observer to be informed when a thread enters or exits
// the scope of ScopedBlockingCall objects.
class BASE_EXPORT BlockingObserver {
 public:
  virtual ~BlockingObserver() = default;

  // Invoked when a ScopedBlockingCall is instantiated on the observed thread
  // where there wasn't an existing ScopedBlockingCall.
  virtual void BlockingStarted(BlockingType blocking_type) = 0;

  // Invoked when a WILL_BLOCK ScopedBlockingCall is instantiated on the
  // observed thread where there was a MAY_BLOCK ScopedBlockingCall but not a
  // WILL_BLOCK ScopedBlockingCall.
  virtual void BlockingTypeUpgraded() = 0;

  // Invoked when the last ScopedBlockingCall on the observed thread is
  // destroyed.
  virtual void BlockingEnded() = 0;
};

// Registers |blocking_observer| on the current thread. It is invalid to call
// this on a thread where there is an active ScopedBlockingCall.
BASE_EXPORT void SetBlockingObserverForCurrentThread(
    BlockingObserver* blocking_observer);

BASE_EXPORT void ClearBlockingObserverForCurrentThread();

// Unregisters the |blocking_observer| on the current thread within its scope.
// Used in ThreadPool tests to prevent calls to //base sync primitives from
// affecting the thread pool capacity.
class BASE_EXPORT ScopedClearBlockingObserverForTesting {
 public:
  ScopedClearBlockingObserverForTesting();
  ~ScopedClearBlockingObserverForTesting();

 private:
  BlockingObserver* const blocking_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedClearBlockingObserverForTesting);
};

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_SCOPED_BLOCKING_CALL_H
