// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_CHROME_ARCORE_INSTALL_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_CHROME_ARCORE_INSTALL_HELPER_H_

#include "base/callback.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"
#include "chrome/browser/vr/vr_export.h"
#include "content/public/browser/xr_install_helper.h"

namespace vr {

// The Actual ArCoreInstallHelper needs an InfoBarManager interface, which
// content/browser is unable to provide, as it has no means of accessing the
// embedder specific factory method, to that end we make a small wrapper class
// to extract the relevant InfoBarManager.
class VR_EXPORT ChromeArCoreInstallHelper : public content::XrInstallHelper {
 public:
  ChromeArCoreInstallHelper();
  ~ChromeArCoreInstallHelper() override;
  ChromeArCoreInstallHelper(const ChromeArCoreInstallHelper&) = delete;
  ChromeArCoreInstallHelper& operator=(const ChromeArCoreInstallHelper&) =
      delete;
  void EnsureInstalled(
      int render_process_id,
      int render_frame_id,
      base::OnceCallback<void(bool)> install_callback) override;

 private:
  ArCoreInstallHelper arcore_install_helper_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_CHROME_ARCORE_INSTALL_HELPER_H_
