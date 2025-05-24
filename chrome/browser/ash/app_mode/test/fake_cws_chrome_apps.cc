// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"

namespace ash::kiosk::test {

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV1() {
  constexpr std::string_view kAppId = "iiigpodgfihagabpagjehoocpakbnclp";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"offline-enabled-chrome-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "_v1.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename = base::StrCat({app_v2.app_id, ".crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2WithPermissionChange() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename =
      base::StrCat({app_v2.app_id, "_v2_permission_change.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption OfflineEnabledChromeAppV2RequiresVersion1234() {
  auto app_v2 = OfflineEnabledChromeAppV1();
  app_v2.crx_filename =
      base::StrCat({app_v2.app_id, "_v2_required_platform_version_added.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption LocalFsChromeAppV1() {
  constexpr std::string_view kAppId = "abbjjkefakmllanciinhgjgjamdmlbdg";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"local-fs-chrome-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, ".crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption LocalFsChromeAppV2() {
  auto app_v2 = LocalFsChromeAppV1();
  app_v2.crx_filename =
      base::StrCat({app_v2.app_id, "_v2_read_and_verify_data.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption MinimumChromeVersionAppV1() {
  constexpr std::string_view kAppId = "ckgconpclkocfoolbepdpgmgaicpegnp";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"minimum-version-chrome-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "-1.0.0.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption
MinimumChromeVersionAppV2WithMinimumVersion100() {
  auto app_v2 = MinimumChromeVersionAppV1();
  app_v2.crx_filename = base::StrCat({app_v2.app_id, "-2.0.0.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption
MinimumChromeVersionAppV3WithMinimumVersion999() {
  auto app_v3 = MinimumChromeVersionAppV1();
  app_v3.crx_filename = base::StrCat({app_v3.app_id, "-3.0.0.crx"});
  app_v3.crx_version = "3.0.0";
  return app_v3;
}

KioskMixin::CwsChromeAppOption AppWithSecondaryAppV1() {
  constexpr std::string_view kAppId = "ilaggnhkinenadmhbbdgbddpaipgfomg";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"app-with-secondary-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "-1.0.0.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption EnterpriseKioskAppV1() {
  constexpr std::string_view kAppId = "gcpjojfkologpegommokeppihdbcnahn";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"enterprise-kiosk-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, ".crx"}),
      /*crx_version=*/"1.0.0"};
}

}  // namespace ash::kiosk::test
