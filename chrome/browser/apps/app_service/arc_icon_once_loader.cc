// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/arc_icon_once_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"

namespace apps {

// A part of an ArcIconOnceLoader, for a specific size_in_dip and
// icon_compression. This two-level structure (an ArcIconOnceLoader contains
// multiple SizeSpecificLoader instances) is needed because each ArcAppIcon is
// for a specific size_in_dip and compressed-ness.
class ArcIconOnceLoader::SizeSpecificLoader : public ArcAppIcon::Observer {
 public:
  SizeSpecificLoader(Profile* profile,
                     int32_t size_in_dip,
                     apps::mojom::IconCompression icon_compression);
  ~SizeSpecificLoader() override;

  void LoadIcon(const std::string& app_id,
                base::OnceCallback<void(ArcAppIcon*)> callback);
  void Remove(const std::string& app_id);
  void Reload(const std::string& app_id, ui::ScaleFactor scale_factor);

  // ArcAppIcon::Observer overrides.
  void OnIconUpdated(ArcAppIcon* icon) override;

 private:
  Profile* const profile_;
  const int32_t size_in_dip_;
  const apps::mojom::IconCompression icon_compression_;

  // Maps App IDs to their icon loaders (for a specific size_in_dip and
  // icon_compression).
  std::map<std::string, std::unique_ptr<ArcAppIcon>> icons_;

  // Maps App IDs to callbacks to run when an icon is completely loaded.
  std::multimap<std::string, base::OnceCallback<void(ArcAppIcon*)>> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SizeSpecificLoader);
};

ArcIconOnceLoader::SizeSpecificLoader::SizeSpecificLoader(
    Profile* profile,
    int32_t size_in_dip,
    apps::mojom::IconCompression icon_compression)
    : profile_(profile),
      size_in_dip_(size_in_dip),
      icon_compression_(icon_compression) {}

ArcIconOnceLoader::SizeSpecificLoader::~SizeSpecificLoader() {
  for (auto& kv_pair : callbacks_) {
    std::move(kv_pair.second).Run(nullptr);
  }
}

void ArcIconOnceLoader::SizeSpecificLoader::LoadIcon(
    const std::string& app_id,
    base::OnceCallback<void(ArcAppIcon*)> callback) {
  auto iter = icons_.find(app_id);
  if ((iter != icons_.end()) &&
      iter->second->EverySupportedScaleFactorIsLoaded()) {
    std::move(callback).Run(iter->second.get());
    return;
  }

  callbacks_.insert(std::make_pair(app_id, std::move(callback)));

  if (iter != icons_.end()) {
    return;
  }
  bool compressed =
      icon_compression_ == apps::mojom::IconCompression::kCompressed;
  iter = icons_
             .insert(std::make_pair(
                 app_id, std::make_unique<ArcAppIcon>(
                             profile_, app_id, size_in_dip_, this, compressed)))
             .first;
  iter->second->LoadSupportedScaleFactors();
}

void ArcIconOnceLoader::SizeSpecificLoader::Remove(const std::string& app_id) {
  auto iter = icons_.find(app_id);
  if (iter != icons_.end()) {
    icons_.erase(iter);
  }
}

void ArcIconOnceLoader::SizeSpecificLoader::Reload(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) {
  auto iter = icons_.find(app_id);
  if (iter != icons_.end()) {
    iter->second->LoadForScaleFactor(scale_factor);
  }
}

void ArcIconOnceLoader::SizeSpecificLoader::OnIconUpdated(ArcAppIcon* icon) {
  if (!icon || !icon->EverySupportedScaleFactorIsLoaded()) {
    return;
  }
  auto range = callbacks_.equal_range(icon->app_id());
  auto count = std::distance(range.first, range.second);
  if (count <= 0) {
    return;
  }

  // Optimize / simplify the common case.
  if (count == 1) {
    base::OnceCallback<void(ArcAppIcon*)> callback =
        std::move(range.first->second);
    callbacks_.erase(range.first, range.second);
    std::move(callback).Run(icon);
    return;
  }

  // Run every callback in |range|. This is subtle, because an arbitrary
  // callback could invoke further methods on |this|, which could mutate
  // |callbacks_|, invalidating |range|'s iterators.
  //
  // Thus, we first gather the callbacks, then erase the |range|, then run the
  // callbacks.
  std::vector<base::OnceCallback<void(ArcAppIcon*)>> callbacks_to_run;
  callbacks_to_run.reserve(count);
  for (auto iter = range.first; iter != range.second; ++iter) {
    callbacks_to_run.push_back(std::move(iter->second));
  }
  callbacks_.erase(range.first, range.second);
  for (auto& callback : callbacks_to_run) {
    std::move(callback).Run(icon);
  }
}

ArcIconOnceLoader::ArcIconOnceLoader(Profile* profile)
    : profile_(profile), stop_observing_called_(false) {
  ArcAppListPrefs::Get(profile)->AddObserver(this);
}

ArcIconOnceLoader::~ArcIconOnceLoader() {
  // Check that somebody called StopObserving. We can't call StopObserving here
  // in the destructor, because we need a ArcAppListPrefs* prefs, and for
  // tests, the prefs pointer for a profile can change over time (e.g. by
  // ArcAppListPrefsFactory::RecreateServiceInstanceForTesting).
  //
  // See also ArcApps::Shutdown.
  DCHECK(stop_observing_called_);
}

void ArcIconOnceLoader::StopObserving(ArcAppListPrefs* prefs) {
  stop_observing_called_ = true;
  if (prefs) {
    prefs->RemoveObserver(this);
  }
}

void ArcIconOnceLoader::LoadIcon(
    const std::string& app_id,
    int32_t size_in_dip,
    apps::mojom::IconCompression icon_compression,
    base::OnceCallback<void(ArcAppIcon*)> callback) {
  auto key = std::make_pair(size_in_dip, icon_compression);
  auto iter = size_specific_loaders_.find(key);
  if (iter == size_specific_loaders_.end()) {
    iter = size_specific_loaders_
               .insert(std::make_pair(
                   key, std::make_unique<SizeSpecificLoader>(
                            profile_, size_in_dip, icon_compression)))
               .first;
  }
  iter->second->LoadIcon(app_id, std::move(callback));
}

void ArcIconOnceLoader::OnAppRemoved(const std::string& app_id) {
  for (auto& iter : size_specific_loaders_) {
    iter.second->Remove(app_id);
  }
}

void ArcIconOnceLoader::OnAppIconUpdated(
    const std::string& app_id,
    const ArcAppIconDescriptor& descriptor) {
  for (int i = 0; i < 2; i++) {
    auto icon_compression = i ? apps::mojom::IconCompression::kCompressed
                              : apps::mojom::IconCompression::kUncompressed;
    auto iter = size_specific_loaders_.find(
        std::make_pair(descriptor.dip_size, icon_compression));
    if (iter != size_specific_loaders_.end()) {
      iter->second->Reload(app_id, descriptor.scale_factor);
    }
  }
}

}  // namespace apps
