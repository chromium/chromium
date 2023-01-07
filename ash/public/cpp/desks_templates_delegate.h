// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_
#define ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "base/time/time.h"
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

  using GetAppLaunchDataCallback =
      base::OnceCallback<void(std::unique_ptr<app_restore::AppLaunchInfo>)>;
  // Gathers the app launch data associated with `window` in order to construct
  // a desk template.  The data is returned via the `callback` that can be
  // called either synchronously or asynchronously, depending on the app.  The
  // callback may receive nullptr if no such app launch data can be constructed,
  // which can happen if the `window` does not have an app id associated with
  // it, or we're not in the primary active user session.
  virtual void GetAppLaunchDataForDeskTemplate(
      aura::Window* window,
      GetAppLaunchDataCallback callback) const = 0;

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
  virtual void GetFaviconForUrl(
      const std::string& page_url,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      base::CancelableTaskTracker* tracker) const = 0;

  // Fetches the icon for the app with `app_id` and returns it via the provided
  // `callback`. `callback` may be called synchronously.
  virtual void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) const = 0;

  // Launches apps into the active desk. Ran immediately after a desk is created
  // for a template.
  virtual void LaunchAppsFromTemplate(
      std::unique_ptr<DeskTemplate> desk_template) = 0;

  // Checks whether `window` is supported in the desks templates feature.
  virtual bool IsWindowSupportedForDeskTemplate(aura::Window* window) const = 0;

  // Return the readable app name for this app id (i.e. "madfksjfasdfkjasdkf" ->
  // "Chrome").
  virtual std::string GetAppShortName(const std::string& app_id) = 0;

  // Return true if the app with the given `app_id` is available to launch from
  // template.
  virtual bool IsAppAvailable(const std::string& app_id) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESKS_TEMPLATES_DELEGATE_H_
