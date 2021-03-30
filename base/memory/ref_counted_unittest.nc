// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"

namespace base {

class InitialRefCountIsZero : public base::RefCounted<InitialRefCountIsZero> {
 public:
  InitialRefCountIsZero() {}
 private:
  friend class base::RefCounted<InitialRefCountIsZero>;
  ~InitialRefCountIsZero() {}
};

#if defined(NCTEST_ADOPT_REF_TO_ZERO_START)  // [r"fatal error: static_assert failed due to requirement 'std::is_same<base::subtle::StartRefCountFromOneTag, base::subtle::StartRefCountFromZeroTag>::value' \"Use AdoptRef only if the reference count starts from one\.\""]

void WontCompile() {
  AdoptRef(new InitialRefCountIsZero());
}

#endif

}  // namespace base
