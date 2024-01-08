// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/icon_loader.h"
#endif

namespace gfx {
class ImageSkia;
}

namespace apps {

constexpr int kSizeInDip = 64;

void EnsureRepresentationsLoaded(gfx::ImageSkia& output_image_skia);

void LoadDefaultIcon(gfx::ImageSkia& output_image_skia,
                     int resource_id = IDR_APP_DEFAULT_ICON);

void VerifyIcon(const gfx::ImageSkia& src, const gfx::ImageSkia& dst);

void VerifyCompressedIcon(const std::vector<uint8_t>& src_data,
                          const apps::IconValue& icon);

gfx::ImageSkia CreateSquareIconImageSkia(int size_dp, SkColor solid_color);

#if BUILDFLAG(IS_CHROMEOS_ASH)
struct AppLaunchParams;

class FakeIconLoader : public apps::IconLoader {
 public:
  explicit FakeIconLoader(apps::AppServiceProxy* proxy);

 private:
  std::unique_ptr<apps::IconLoader::Releaser> LoadIconFromIconKey(
      const std::string& id,
      const apps::IconKey& icon_key,
      apps::IconType icon_type,
      int32_t size_in_dip,
      bool allow_placeholder_icon,
      apps::LoadIconCallback callback) override;

  raw_ptr<apps::AppServiceProxy> proxy_ = nullptr;
};

class FakePublisherForIconTest : public apps::AppPublisher {
 public:
  FakePublisherForIconTest(apps::AppServiceProxy* proxy,
                           apps::AppType app_type);

  ~FakePublisherForIconTest() override = default;

  void Launch(const std::string& app_id,
              int32_t event_flags,
              apps::LaunchSource launch_source,
              apps::WindowInfoPtr window_info) override {}

  void LaunchAppWithParams(apps::AppLaunchParams&& params,
                           apps::LaunchCallback callback) override {}

  void LoadIcon(const std::string& app_id,
                const apps::IconKey& icon_key,
                apps::IconType icon_type,
                int32_t size_hint_in_dip,
                bool allow_placeholder_icon,
                apps::LoadIconCallback callback) override {}

  void GetCompressedIconData(const std::string& app_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback) override;
};
#endif

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_APP_ICON_TEST_UTIL_H_
