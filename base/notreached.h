// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NOTREACHED_H_
#define BASE_NOTREACHED_H_

#include "base/base_export.h"
#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/logging_buildflags.h"

namespace logging {

// On DCHECK builds NOTREACHED() match the fatality of DCHECKs. When DCHECKs are
// non-FATAL a crash report will be generated for the first NOTREACHED() that
// hits per process.
//
// Outside DCHECK builds NOTREACHED() will LOG(ERROR) and also upload a crash
// report without crashing in order to weed out prevalent NOTREACHED()s in the
// wild before always turning NOTREACHED()s FATAL.
//
// TODO(crbug.com/851128): Turn NOTREACHED() FATAL and mark them [[noreturn]].
#if CHECK_WILL_STREAM() || BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
#define NOTREACHED()   \
  CHECK_FUNCTION_IMPL( \
      ::logging::NotReachedError::NotReached(__FILE__, __LINE__), false)
#else
#define NOTREACHED()                                       \
  (true) ? ::logging::NotReachedError::TriggerNotReached() \
         : EAT_CHECK_STREAM_PARAMS()
#endif

// The NOTIMPLEMENTED() macro annotates codepaths which have not been
// implemented yet. If output spam is a serious concern,
// NOTIMPLEMENTED_LOG_ONCE can be used.
// Note that the NOTIMPLEMENTED_LOG_ONCE() macro does not allow custom error
// messages to be appended to the macro to log, unlike NOTIMPLEMENTED() which
// does support the pattern of appending a custom error message.  As in, the
// NOTIMPLEMENTED_LOG_ONCE() << "foo message"; pattern is not supported.
#if DCHECK_IS_ON()
#define NOTIMPLEMENTED() \
  ::logging::CheckError::NotImplemented(__FILE__, __LINE__, __PRETTY_FUNCTION__)
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
  }

}  // namespace logging

#endif  // BASE_NOTREACHED_H_
