// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_ANDROID_H_

#include "chrome/browser/nfc/nfc_permission_context.h"
#include "components/permissions/android/nfc/nfc_system_level_setting.h"

namespace permissions {
class PermissionRequestID;
}

class NfcPermissionContextAndroid : public NfcPermissionContext {
 public:
  explicit NfcPermissionContextAndroid(
      content::BrowserContext* browser_context);
  ~NfcPermissionContextAndroid() override;

 private:
  friend class NfcPermissionContextTests;
  friend class PermissionManagerTest;

  // NfcPermissionContext:
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting) override;

  void OnNfcSystemLevelSettingPromptClosed(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      ContentSetting content_setting);

  // Overrides the NfcSystemLevelSetting object used to determine whether NFC is
  // enabled system-wide on the device.
  void set_nfc_system_level_setting_for_testing(
      std::unique_ptr<permissions::NfcSystemLevelSetting>
          nfc_system_level_setting) {
    nfc_system_level_setting_ = std::move(nfc_system_level_setting);
  }

  std::unique_ptr<permissions::NfcSystemLevelSetting> nfc_system_level_setting_;

  base::WeakPtrFactory<NfcPermissionContextAndroid> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NFC_NFC_PERMISSION_CONTEXT_ANDROID_H_
