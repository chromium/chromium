// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ICON_ONCE_LOADER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ICON_ONCE_LOADER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class ArcAppIcon;
class Profile;

namespace apps {

// An icon loader specific to ARC++ apps, similar to the ArcAppIconLoader
// class, except that it works with base::OnceCallback's (such as those used by
// Mojo IPC). As the name implies, a base::OnceCallback can be run only once.
// The ArcAppIconLoader class, like any AppIconLoader sub-class, assumes that
// AppIconLoaderDelegate::OnAppImageUpdated can be called multiple times.
//
// Another minor difference is that this class works with multiple icon sizes.
// Each AppIconLoader instance is for only one icon size.
class ArcIconOnceLoader : public ArcAppListPrefs::Observer {
 public:
  // The constructor caller is responsible for calling StopObserver before
  // destroying this.
  explicit ArcIconOnceLoader(Profile* profile);
  ArcIconOnceLoader(const ArcIconOnceLoader&) = delete;
  ArcIconOnceLoader& operator=(const ArcIconOnceLoader&) = delete;
  ~ArcIconOnceLoader() override;

  void StopObserving(ArcAppListPrefs* prefs);

  // Runs |callback| when the corresponding ARC++ icon is completely loaded,
  // which may occur before LoadIcon returns, if that icon is cached.
  //
  // The callback may be run with a nullptr argument, if the icon could not be
  // loaded.
  void LoadIcon(const std::string& app_id,
                int32_t size_in_dip,
                apps::IconType icon_type,
                base::OnceCallback<void(ArcAppIcon*)> callback);

  // ArcAppListPrefs::Observer overrides.
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override;

  void SetArcAppIconFactoryForTesting(
      std::unique_ptr<arc::ArcAppIconFactory> arc_app_icon_factory);

  arc::ArcAppIconFactory* arc_app_icon_factory() {
    return arc_app_icon_factory_.get();
  }

 private:
  class SizeSpecificLoader;

  using SizeAndType = std::pair<int32_t, apps::IconType>;

  // When loading many app icons, there could be many icon files opened at the
  // same time, which might cause the system crash. So checking the current icon
  // loading request number, and if there are too many requests, add the
  // ArcAppIcon to |pending_requests_| to load the icon later, and return
  // false. Otherwise add the ArcAppIcon to |in_flight_requests_| so that we
  // can calculate how many in flight icon loading requests, and return true.
  void MaybeStartIconRequest(ArcAppIcon* arc_app_icon,
                             ui::ResourceScaleFactor scale_factor);

  // When get the reply from |arc_app_icon| or remove the app, remove
  // |arc_app_icon| from |in_flight_requests_| and |pending_requests_|, and
  // start loading icon requests from |pending_requests_|.
  void RemoveArcAppIcon(ArcAppIcon* arc_app_icon);

  // If there are loading icon requests saved in |pending_requests_|,
  // and not too many requests, get ArcAppIcon from
  // |pending_requests_| to load icons.
  void MaybeLoadPendingIconRequest();

  Profile* const profile_;
  bool stop_observing_called_ = false;
  std::map<SizeAndType, std::unique_ptr<SizeSpecificLoader>>
      size_specific_loaders_;

  std::unique_ptr<arc::ArcAppIconFactory> arc_app_icon_factory_;

  // The current icon loading requests.
  std::set<ArcAppIcon*> in_flight_requests_;

  // The ArcAppIcon map to record the pending icon loading requests.
  std::map<ArcAppIcon*, std::set<ui::ResourceScaleFactor>> pending_requests_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_ARC_ICON_ONCE_LOADER_H_
