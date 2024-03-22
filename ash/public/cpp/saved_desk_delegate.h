// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SAVED_DESK_DELEGATE_H_
#define ASH_PUBLIC_CPP_SAVED_DESK_DELEGATE_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

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
class AdminTemplateService;
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
// DesksClient in chrome/.
class ASH_PUBLIC_EXPORT SavedDeskDelegate {
 public:
  virtual ~SavedDeskDelegate() = default;

  using GetAppLaunchDataCallback =
      base::OnceCallback<void(std::unique_ptr<app_restore::AppLaunchInfo>)>;
  // Gathers the app launch data associated with `window` in order to construct
  // a saved desk.  The data is returned via the `callback` that can be called
  // either synchronously or asynchronously, depending on the app.  The callback
  // may receive nullptr if no such app launch data can be constructed, which
  // can happen if the `window` does not have an app id associated with it, or
  // we're not in the primary active user session.
  virtual void GetAppLaunchDataForSavedDesk(
      aura::Window* window,
      GetAppLaunchDataCallback callback) const = 0;

  // Returns either the local desk storage backend or Chrome sync desk storage
  // backend depending on the feature flag DeskTemplateSync.
  virtual desks_storage::DeskModel* GetDeskModel() = 0;

  // returns the appropriate AdminTemplateService for the active profile.
  virtual desks_storage::AdminTemplateService* GetAdminTemplateService() = 0;

  // Returns whether `window` is persistable.  If true the window should be
  // tracked and saved as part of the desk.  If false, this window should
  // be ignored.
  // TODO(sammiequon|minch) : Move and rename this or add a new function
  // `IsNonRegularProfileWindow` inside shell delegate to check whether the
  // `window` is an incognito ash browser window or a lacros window with the
  // non-regular profile.
  virtual bool IsWindowPersistable(aura::Window* window) const = 0;

  // Returns the corresponding icon for `icon_identifier` if it's a special
  // identifier. I.e. NTP or incognito window. If `icon_identifier` is not a
  // special identifier, return `asbl::nullopt`. `color_provider` should be the
  // ui::ColorProvider corresponding to an incognito window or nullptr.
  virtual std::optional<gfx::ImageSkia> MaybeRetrieveIconForSpecialIdentifier(
      const std::string& icon_identifier,
      const ui::ColorProvider* color_provider) const = 0;

  // Fetches the favicon for `page_url` and returns it via the provided
  // `callback`. When lacros is active, the profile identified by
  // `lacros_profile_id` is used to get the favicon. `callback` may be called
  // synchronously.
  virtual void GetFaviconForUrl(
      const std::string& page_url,
      uint64_t lacros_profile_id,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      base::CancelableTaskTracker* tracker) const = 0;

  // Fetches the icon for the app with `app_id` and returns it via the provided
  // `callback`. `callback` may be called synchronously.
  // TODO(sammiequon): This is used for other features, migrate to shell
  // delegate.
  virtual void GetIconForAppId(
      const std::string& app_id,
      int desired_icon_size,
      base::OnceCallback<void(const gfx::ImageSkia&)> callback) const = 0;

  // Launches apps into the active desk. Ran immediately after a desk is created
  // for a saved desk.
  virtual void LaunchAppsFromSavedDesk(
      std::unique_ptr<DeskTemplate> saved_desk) = 0;

  // Checks whether `window` is supported in the desk templates feature or the
  // save and recall feature.
  virtual bool IsWindowSupportedForSavedDesk(aura::Window* window) const = 0;

  // Return the readable app name for this app id (i.e. "madfksjfasdfkjasdkf" ->
  // "Chrome").
  virtual std::string GetAppShortName(const std::string& app_id) = 0;

  // Return true if the app with the given `app_id` is available to launch from
  // the saved desk.
  virtual bool IsAppAvailable(const std::string& app_id) const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SAVED_DESK_DELEGATE_H_
