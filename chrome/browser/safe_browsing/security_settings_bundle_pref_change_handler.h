// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_PREF_CHANGE_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_PREF_CHANGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

class Profile;
class ToastController;

namespace safe_browsing {

// Handles showing the appropriate toast or modal when the security
// settings bundle changes. This class is not thread-safe.
class SecuritySettingsBundlePrefChangeHandler {
 public:
  explicit SecuritySettingsBundlePrefChangeHandler(Profile* profile);
  virtual ~SecuritySettingsBundlePrefChangeHandler();
  virtual void MaybeShowEnhancedBundleSettingChangeNotification();

 private:
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
  ToastController* GetToastController();
#endif

  // Member variable to store the Profile*.
  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SECURITY_SETTINGS_BUNDLE_PREF_CHANGE_HANDLER_H_
