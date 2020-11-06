// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRITICAL_CLOSURE_H_
#define BASE_CRITICAL_CLOSURE_H_

#include <utility>

#include "base/callback.h"
#include "base/location.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#include "base/bind.h"
#include "base/ios/scoped_critical_action.h"
#endif

namespace base {

namespace internal {

#if defined(OS_IOS)
// Returns true if multi-tasking is supported on this iOS device.
bool IsMultiTaskingSupported();

// This class wraps a closure so it can continue to run for a period of time
// when the application goes to the background by using
// |ios::ScopedCriticalAction|.
class CriticalClosure {
 public:
  explicit CriticalClosure(StringPiece task_name, OnceClosure closure);
  CriticalClosure(const CriticalClosure&) = delete;
  CriticalClosure& operator=(const CriticalClosure&) = delete;
  ~CriticalClosure();
  void Run();

 private:
  ios::ScopedCriticalAction critical_action_;
  OnceClosure closure_;
};
#endif  // defined(OS_IOS)

}  // namespace internal

// Returns a closure that will continue to run for a period of time when the
// application goes to the background if possible on platforms where
// applications don't execute while backgrounded, otherwise the original task is
// returned.
//
// Example:
//   file_task_runner_->PostTask(
//       FROM_HERE,
//       MakeCriticalClosure(task_name,
//                           base::BindOnce(&WriteToDiskTask, path_, data)));
//
// Note new closures might be posted in this closure. If the new closures need
// background running time, |MakeCriticalClosure| should be applied on them
// before posting. |task_name| is used by the platform to identify any tasks
// that do not complete in time for suspension.
#if defined(OS_IOS)
inline OnceClosure MakeCriticalClosure(StringPiece task_name,
                                       OnceClosure closure) {
  DCHECK(internal::IsMultiTaskingSupported());
  return base::BindOnce(
      &internal::CriticalClosure::Run,
      Owned(new internal::CriticalClosure(task_name, std::move(closure))));
}

inline OnceClosure MakeCriticalClosure(const Location& posted_from,
                                       OnceClosure closure) {
  return MakeCriticalClosure(posted_from.ToString(), std::move(closure));
}

#else  // defined(OS_IOS)
inline OnceClosure MakeCriticalClosure(StringPiece task_name,
                                       OnceClosure closure) {
  // No-op for platforms where the application does not need to acquire
  // background time for closures to finish when it goes into the background.
  return closure;
}

inline OnceClosure MakeCriticalClosure(const Location& posted_from,
                                       OnceClosure closure) {
  return closure;
}

#endif  // defined(OS_IOS)

}  // namespace base

#endif  // BASE_CRITICAL_CLOSURE_H_
