// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_ARC_ICON_ONCE_LOADER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_ARC_ICON_ONCE_LOADER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_descriptor.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

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
  ~ArcIconOnceLoader() override;

  void StopObserving(ArcAppListPrefs* prefs);

  // Runs |callback| when the corresponding ARC++ icon is completely loaded,
  // which may occur before LoadIcon returns, if that icon is cached.
  //
  // The callback may be run with a nullptr argument, if the icon could not be
  // loaded.
  void LoadIcon(const std::string& app_id,
                int32_t size_in_dip,
                apps::mojom::IconCompression icon_compression,
                base::OnceCallback<void(ArcAppIcon*)> callback);

  // ArcAppListPrefs::Observer overrides.
  void OnAppRemoved(const std::string& app_id) override;
  void OnAppIconUpdated(const std::string& app_id,
                        const ArcAppIconDescriptor& descriptor) override;

 private:
  class SizeSpecificLoader;

  using SizeAndCompression = std::pair<int32_t, apps::mojom::IconCompression>;

  Profile* const profile_;
  bool stop_observing_called_;
  std::map<SizeAndCompression, std::unique_ptr<SizeSpecificLoader>>
      size_specific_loaders_;

  DISALLOW_COPY_AND_ASSIGN(ArcIconOnceLoader);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_ARC_ICON_ONCE_LOADER_H_
