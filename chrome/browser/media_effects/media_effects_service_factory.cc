// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_effects/media_effects_service_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"
#include "services/video_effects/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
#include "components/media_effects/media_effects_model_provider.h"
#endif

// static
MediaEffectsService* MediaEffectsServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<MediaEffectsService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
MediaEffectsServiceFactory* MediaEffectsServiceFactory::GetInstance() {
  static base::NoDestructor<MediaEffectsServiceFactory> instance;

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // Media Effects Service depends on Optimization Guide because it needs to
  // subscribe to the model updates.
  instance->DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
#endif

  return instance.get();
}

MediaEffectsServiceFactory::MediaEffectsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "MediaEffectsServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {}

MediaEffectsServiceFactory::~MediaEffectsServiceFactory() = default;

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
class SegmentationModelObserver
    : public optimization_guide::OptimizationTargetModelObserver,
      public MediaEffectsModelProvider {
 public:
  explicit SegmentationModelObserver(content::BrowserContext* browser_context) {
    // It's safe to store the Optimization Guide service by raw pointer - the
    // `SegmentationModelObserver` will be owned by `MediaEffectsService`, and
    // we add a dependency between `OptimizationGuideKeyedServiceFactory` &
    // `MediaEffectsServiceFactory`, which means that Media Effects Service (and
    // by extension this observer) will be destroyed before Optimization Guide.
    optimization_guide_ = OptimizationGuideKeyedServiceFactory::GetForProfile(
        Profile::FromBrowserContext(browser_context));
    if (!optimization_guide_) {
      // Optimization Guide can be disabled by a feature, so we should tolerate
      // it being null.
      return;
    }

    optimization_guide_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION,
        std::nullopt, this);
  }

  ~SegmentationModelObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!optimization_guide_) {
      return;
    }

    optimization_guide_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION,
        this);
  }

  SegmentationModelObserver(const SegmentationModelObserver& other) = delete;
  SegmentationModelObserver& operator=(const SegmentationModelObserver& other) =
      delete;

  SegmentationModelObserver(SegmentationModelObserver&& other) = delete;
  SegmentationModelObserver& operator=(SegmentationModelObserver&& other) =
      delete;

  // optimization_guide::OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // We only subscribe to notifications for
    // `OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION`, so we know that the
    // updated model info pertains to background segmentation models:
    CHECK_EQ(optimization_target,
             optimization_guide::proto::OptimizationTarget::
                 OPTIMIZATION_TARGET_CAMERA_BACKGROUND_SEGMENTATION);

    background_segmentation_model_path_ =
        model_info.has_value() ? std::optional(model_info->GetModelFilePath())
                               : std::nullopt;

    for (auto& observer : observers_) {
      observer.OnBackgroundSegmentationModelUpdated(
          background_segmentation_model_path_);
    }
  }

  // MediaEffectsModelProvider:
  void AddObserver(MediaEffectsModelProvider::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // If we already have a model path, let's inform the new observer about it.
    if (background_segmentation_model_path_) {
      observer->OnBackgroundSegmentationModelUpdated(
          *background_segmentation_model_path_);
    }

    observers_.AddObserver(observer);
  }

  void RemoveObserver(MediaEffectsModelProvider::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    observers_.RemoveObserver(observer);
  }

 private:
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_;
  base::ObserverList<MediaEffectsModelProvider::Observer> observers_;
  std::optional<base::FilePath> background_segmentation_model_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};
#endif

std::unique_ptr<KeyedService>
MediaEffectsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  CHECK(browser_context);

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  std::unique_ptr<SegmentationModelObserver> model_provider;
  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    model_provider =
        std::make_unique<SegmentationModelObserver>(browser_context);
  }
  return std::make_unique<MediaEffectsService>(
      user_prefs::UserPrefs::Get(browser_context), std::move(model_provider));
#else
  return std::make_unique<MediaEffectsService>(
      user_prefs::UserPrefs::Get(browser_context));
#endif
}
