// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_MODULES_EXTRA_ICU_PROVIDER_MODULE_PROVIDER_H_
#define CHROME_ANDROID_MODULES_EXTRA_ICU_PROVIDER_MODULE_PROVIDER_H_

namespace extra_icu {

// Native side of the extra ICU module installer.
class ModuleProvider {
 public:
  // Returns true if the extra ICU module is installed.
  static bool IsModuleInstalled();
};

}  // namespace extra_icu

#endif  // CHROME_ANDROID_MODULES_EXTRA_ICU_PROVIDER_MODULE_PROVIDER_H_
