// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"

namespace ash::kiosk::test {

namespace {

constexpr std::string_view kOfflineEnabledKioskAppId =
    "iiigpodgfihagabpagjehoocpakbnclp";

constexpr std::string_view kLocalFsAppId = "abbjjkefakmllanciinhgjgjamdmlbdg";

}  // namespace

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV1() {
  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"offline-enabled-chrome-app@localhost",
      /*app_id=*/kOfflineEnabledKioskAppId,
      /*crx_filename=*/base::StrCat({kOfflineEnabledKioskAppId, "_v1.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename = base::StrCat({kOfflineEnabledKioskAppId, ".crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2WithPermissionChange() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename =
      base::StrCat({kOfflineEnabledKioskAppId, "_v2_permission_change.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2RequiresVersion1234() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename = base::StrCat(
      {kOfflineEnabledKioskAppId, "_v2_required_platform_version_added.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption LocalFsChromeAppV1() {
  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"local-fs-chrome-app@localhost",
      /*app_id=*/kLocalFsAppId,
      /*crx_filename=*/base::StrCat({kLocalFsAppId, ".crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption LocalFsChromeAppV2() {
  auto app_v2 = LocalFsChromeAppV1();
  app_v2.crx_filename =
      base::StrCat({kLocalFsAppId, "_v2_read_and_verify_data.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

}  // namespace ash::kiosk::test
