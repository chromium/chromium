// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_
#define ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_restore {
struct AppLaunchInfo;
}

namespace aura {
class Window;
}

namespace base {
class CancelableTaskTracker;
}

namespace desks_storage {
class DeskModel;
}

namespace gfx {
class ImageSkia;
}

namespace ui {
class ColorProvider;
}

namespace ash {

class DeskTemplate;

// This delegate is owned by Shell and used by ash/ to communicate with
// DesksTemplatesClient in chrome/.
class ASH_PUBLIC_EXPORT DesksTemplatesDelegate {
 public:
  virtual ~DesksTemplatesDelegate() = default;

  // Returns the app launch data that's associated with a particular `window` in
  // order to construct a desk template. Return nullptr if no such app launch
  // data can be constructed, which can happen if the `window` does not have
  // an app id associated with it, or we're not in the primary active user
  // session.
  virtual std::unique_ptr<app_restore::AppLaunchInfo>
  GetAppLaunchDataForDeskTemplate(aura::Window* window) const = 0;

  // Returns either the local desk storage backend or Chrome sync desk storage
  // backend depending on the feature flag DeskTemplateSync.
  virtual desks_storage::DeskModel* GetDeskModel() = 0;

  // Returns whether `window` is an incognito browser.
  virtual bool IsIncognitoWindow(aura::Window* window) const = 0;

  // Returns the corresponding icon for `icon_identifier` if it's a special
  // identifier. I.e. NTP or incognito window. If `icon_identifier` is not a
  // special identifier, return `asbl::nullopt`. `color_provider` should be the
  // ui::ColorProvider corresponding to an incognito window or nullptr.
  virtual absl::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& icon_identifier,
      const ui::ColorProvider* color_provider) const = 0;

  // Fetches the favicon for `page_url` and returns it via the provided
  // `callback`. `callback` may be called synchronously.
  virtual void GetFaviconForUrl(const std::string& page_url,
                                int desired_icon_size,
                                favicon_base::FaviconRawBitmapCallback callback,
                                base::CancelableTaskTracker* tracker) const = 0;

  // Fetches the icon for the app with `app_id` and returns it via the provided
  // `callback`. `callback` may be called synchronously.
  virtual void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(apps::IconValuePtr icon_value)> callback)
      const = 0;

  // Launches apps into the active desk. Ran immediately after a desk is created
  // for a template.
  virtual void LaunchAppsFromTemplate(
      std::unique_ptr<DeskTemplate> desk_template) = 0;

  // Checks whether `window` is supported in the desks templates feature.
  virtual bool IsWindowSupportedForDeskTemplate(aura::Window* window) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_
