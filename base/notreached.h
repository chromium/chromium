// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTREACHED_H_
#define BASE_NOTREACHED_H_

#include "base/check.h"
#include "base/logging_buildflags.h"

namespace logging {

#if BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
void BASE_EXPORT LogErrorNotReached(const char* file, int line);
#define NOTREACHED()                                       \
  true ? ::logging::LogErrorNotReached(__FILE__, __LINE__) \
       : EAT_CHECK_STREAM_PARAMS()
#else
#define NOTREACHED() DCHECK(false)
#endif

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE can be used.
#if DCHECK_IS_ON()
#define NOTIMPLEMENTED()                                     \
  ::logging::CheckError::NotImplemented(__FILE__, __LINE__,  \
                                        __PRETTY_FUNCTION__) \
      .stream()
#else
#define NOTIMPLEMENTED() EAT_CHECK_STREAM_PARAMS()
#endif

#define NOTIMPLEMENTED_LOG_ONCE()    \
  {                                  \
    static bool logged_once = false; \
    if (!logged_once) {              \
      NOTIMPLEMENTED();              \
      logged_once = true;            \
    }                                \
  }                                  \
  EAT_CHECK_STREAM_PARAMS()

}  // namespace logging

#endif  // BASE_NOTREACHED_H_
