// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCOPED_TEST_MV2_ENABLER_H_
#define CHROME_BROWSER_EXTENSIONS_SCOPED_TEST_MV2_ENABLER_H_

#include "base/auto_reset.h"
#include "base/types/pass_key.h"

namespace extensions {

// A utility to allow loading and enabling legacy MV2 extensions. This class
// does *not* force the individual field trials for the MV2 deprecation to any
// particular state, since, for maximum test coverage, those should continue to
// run on many different configurations. Instead, this only prevents MV2
// extensions from being disabled on startup or blocked from installation.
class ScopedTestMV2Enabler {
 public:
  ScopedTestMV2Enabler();
  ~ScopedTestMV2Enabler();

 private:
  using PassKey = base::PassKey<ScopedTestMV2Enabler>;

  base::AutoReset<bool> enable_mv2_extensions_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCOPED_TEST_MV2_ENABLER_H_
