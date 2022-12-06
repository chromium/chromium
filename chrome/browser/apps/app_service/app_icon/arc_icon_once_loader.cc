// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_icon/arc_icon_once_loader.h"

#include <vector>

#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/profiles/profile.h"

namespace apps {

constexpr size_t kMaxSimultaneousIconRequests = 250;

// A part of an ArcIconOnceLoader, for a specific size_in_dip and
// icon_type. This two-level structure (an ArcIconOnceLoader contains
// multiple SizeSpecificLoader instances) is needed because each ArcAppIcon is
// for a specific size_in_dip and type.
class ArcIconOnceLoader::SizeSpecificLoader : public ArcAppIcon::Observer {
 public:
  SizeSpecificLoader(Profile* profile,
                     int32_t size_in_dip,
                     apps::IconType icon_type,
                     ArcIconOnceLoader& host);
  SizeSpecificLoader(const SizeSpecificLoader&) = delete;
  SizeSpecificLoader& operator=(const SizeSpecificLoader&) = delete;
  ~SizeSpecificLoader() override;

  void LoadIcon(const std::string& app_id,
                base::OnceCallback<void(ArcAppIcon*)> callback);
  void Remove(const std::string& app_id);
  void Reload(const std::string& app_id, ui::ResourceScaleFactor scale_factor);

  // ArcAppIcon::Observer overrides.
  void OnIconUpdated(ArcAppIcon* icon) override;
  void OnIconFailed(ArcAppIcon* icon) override;

 private:
  Profile* const profile_;
  const int32_t size_in_dip_;
  const apps::IconType icon_type_;
  ArcIconOnceLoader& host_;

  // Maps App IDs to their icon loaders (for a specific size_in_dip and
  // icon_compression).
  std::map<std::string, std::unique_ptr<ArcAppIcon>> icons_;

  // Maps App IDs to callbacks to run when an icon is completely loaded.
  std::multimap<std::string, base::OnceCallback<void(ArcAppIcon*)>> callbacks_;
};

ArcIconOnceLoader::SizeSpecificLoader::SizeSpecificLoader(
    Profile* profile,
    int32_t size_in_dip,
    apps::IconType icon_type,
    ArcIconOnceLoader& host)
    : profile_(profile),
      size_in_dip_(size_in_dip),
      icon_type_(icon_type),
      host_(host) {}

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
  ArcAppIcon::IconType icon_type;
  switch (icon_type_) {
    case apps::IconType::kUnknown:
    case apps::IconType::kUncompressed:
      icon_type = ArcAppIcon::IconType::kAdaptive;
      break;
    case apps::IconType::kCompressed:
      icon_type = ArcAppIcon::IconType::kAdaptive;
      break;
    case apps::IconType::kStandard:
      icon_type = ArcAppIcon::IconType::kAdaptive;
      break;
  }

  auto arc_app_icon = host_.arc_app_icon_factory()->CreateArcAppIcon(
      profile_, app_id, size_in_dip_, this, icon_type);
  iter = icons_.insert(std::make_pair(app_id, std::move(arc_app_icon))).first;
  host_.MaybeStartIconRequest(iter->second.get(),
                              ui::ResourceScaleFactor::NUM_SCALE_FACTORS);
  return;
}

void ArcIconOnceLoader::SizeSpecificLoader::Remove(const std::string& app_id) {
  auto iter = icons_.find(app_id);
  if (iter != icons_.end()) {
    host_.RemoveArcAppIcon(iter->second.get());
    icons_.erase(iter);
  }
}

void ArcIconOnceLoader::SizeSpecificLoader::Reload(
    const std::string& app_id,
    ui::ResourceScaleFactor scale_factor) {
  auto iter = icons_.find(app_id);
  if (iter != icons_.end()) {
    host_.MaybeStartIconRequest(iter->second.get(), scale_factor);
    return;
  }
}

void ArcIconOnceLoader::SizeSpecificLoader::OnIconUpdated(ArcAppIcon* icon) {
  if (!icon) {
    return;
  }

  host_.RemoveArcAppIcon(icon);

  if (!icon->EverySupportedScaleFactorIsLoaded()) {
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

void ArcIconOnceLoader::SizeSpecificLoader::OnIconFailed(ArcAppIcon* icon) {
  OnIconUpdated(icon);
}

ArcIconOnceLoader::ArcIconOnceLoader(Profile* profile) : profile_(profile) {
  ArcAppListPrefs::Get(profile)->AddObserver(this);
  arc_app_icon_factory_ = std::make_unique<arc::ArcAppIconFactory>();
}

ArcIconOnceLoader::~ArcIconOnceLoader() {
  // Check that somebody called StopObserving. We can't call StopObserving
  // here in the destructor, because we need a ArcAppListPrefs* prefs, and for
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
    apps::IconType icon_type,
    base::OnceCallback<void(ArcAppIcon*)> callback) {
  auto key = std::make_pair(size_in_dip, icon_type);
  auto iter = size_specific_loaders_.find(key);
  if (iter == size_specific_loaders_.end()) {
    iter = size_specific_loaders_
               .insert(std::make_pair(
                   key, std::make_unique<SizeSpecificLoader>(
                            profile_, size_in_dip, icon_type, *this)))
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
  for (int i = static_cast<int>(apps::IconType::kUncompressed);
       i <= static_cast<int>(apps::IconType::kStandard); ++i) {
    auto iter = size_specific_loaders_.find(
        std::make_pair(descriptor.dip_size, static_cast<apps::IconType>(i)));
    if (iter != size_specific_loaders_.end()) {
      iter->second->Reload(app_id, descriptor.scale_factor);
    }
  }
}

void ArcIconOnceLoader::SetArcAppIconFactoryForTesting(
    std::unique_ptr<arc::ArcAppIconFactory> arc_app_icon_factory) {
  arc_app_icon_factory_ = std::move(arc_app_icon_factory);
}

void ArcIconOnceLoader::MaybeStartIconRequest(
    ArcAppIcon* arc_app_icon,
    ui::ResourceScaleFactor scale_factor) {
  DCHECK(arc_app_icon);
  if (in_flight_requests_.size() < kMaxSimultaneousIconRequests) {
    in_flight_requests_.insert(arc_app_icon);
    if (scale_factor == ui::ResourceScaleFactor::NUM_SCALE_FACTORS) {
      arc_app_icon->LoadSupportedScaleFactors();
    } else {
      arc_app_icon->LoadForScaleFactor(scale_factor);
    }
    return;
  }

  pending_requests_[arc_app_icon].insert(scale_factor);
}

void ArcIconOnceLoader::RemoveArcAppIcon(ArcAppIcon* arc_app_icon) {
  DCHECK(arc_app_icon);

  in_flight_requests_.erase(arc_app_icon);
  pending_requests_.erase(arc_app_icon);

  MaybeLoadPendingIconRequest();
}

void ArcIconOnceLoader::MaybeLoadPendingIconRequest() {
  while (!pending_requests_.empty() &&
         in_flight_requests_.size() < kMaxSimultaneousIconRequests) {
    auto it = pending_requests_.begin();

    ArcAppIcon* arc_app_icon = it->first;
    DCHECK(arc_app_icon);

    std::set<ui::ResourceScaleFactor>& scale_factors = it->second;
    DCHECK(!scale_factors.empty());

    // Handle all pending icon loading requests for |arc_app_icon|.
    for (auto scale_factor : scale_factors) {
      if (scale_factor == ui::ResourceScaleFactor::NUM_SCALE_FACTORS) {
        arc_app_icon->LoadSupportedScaleFactors();
      } else {
        arc_app_icon->LoadForScaleFactor(scale_factor);
      }
    }
    in_flight_requests_.insert(arc_app_icon);

    pending_requests_.erase(it);
  }
}

}  // namespace apps
