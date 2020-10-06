// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/chrome_arcore_install_helper.h"

#include "chrome/browser/android/vr/android_vr_utils.h"
#include "chrome/browser/android/vr/arcore_device/arcore_install_helper.h"
#include "chrome/browser/infobars/infobar_service.h"

using base::android::AttachCurrentThread;

namespace vr {
ChromeArCoreInstallHelper::ChromeArCoreInstallHelper() = default;
ChromeArCoreInstallHelper::~ChromeArCoreInstallHelper() = default;

void ChromeArCoreInstallHelper::EnsureInstalled(
    int render_process_id,
    int render_frame_id,
    base::OnceCallback<void(bool)> install_callback) {
  auto* infobar_manager = InfoBarService::FromWebContents(
      GetWebContents(render_process_id, render_frame_id));
  DCHECK(infobar_manager);
  arcore_install_helper_.EnsureInstalled(render_process_id, render_frame_id,
                                         infobar_manager,
                                         std::move(install_callback));
}

}  // namespace vr
