// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILING_HOST_CHROME_BROWSER_MAIN_EXTRA_PARTS_PROFILING_H_
#define CHROME_BROWSER_PROFILING_HOST_CHROME_BROWSER_MAIN_EXTRA_PARTS_PROFILING_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeBrowserMainExtraPartsProfiling
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsProfiling();
  ~ChromeBrowserMainExtraPartsProfiling() override;

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsProfiling);
};

#endif  // CHROME_BROWSER_PROFILING_HOST_CHROME_BROWSER_MAIN_EXTRA_PARTS_PROFILING_H_
