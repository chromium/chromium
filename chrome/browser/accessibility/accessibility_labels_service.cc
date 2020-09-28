// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/language_usage_metrics/language_usage_metrics.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/common/content_features.h"
#include "google_apis/google_api_keys.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/image_annotation/image_annotation_service.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#endif

using LanguageInfo = language::UrlLanguageHistogram::LanguageInfo;

namespace {

// Returns the Chrome Google API key for the channel of this build.
std::string APIKeyForChannel() {
  if (chrome::GetChannel() == version_info::Channel::STABLE)
    return google_apis::GetAPIKey();
  return google_apis::GetNonStableAPIKey();
}

AccessibilityLabelsService::ImageAnnotatorBinder&
GetImageAnnotatorBinderOverride() {
  static base::NoDestructor<AccessibilityLabelsService::ImageAnnotatorBinder>
      binder;
  return *binder;
}

class ImageAnnotatorClient : public image_annotation::Annotator::Client {
 public:
  explicit ImageAnnotatorClient(Profile* profile) : profile_(profile) {}
  ~ImageAnnotatorClient() override = default;

  // image_annotation::Annotator::Client implementation:
  void BindJsonParser(mojo::PendingReceiver<data_decoder::mojom::JsonParser>
                          receiver) override {
    data_decoder_.GetService()->BindJsonParser(std::move(receiver));
  }

  std::vector<std::string> GetAcceptLanguages() override {
    std::vector<std::string> accept_languages;
    const PrefService* pref_service = profile_->GetPrefs();
    std::string accept_languages_pref =
        pref_service->GetString(language::prefs::kAcceptLanguages);
    for (std::string lang :
         base::SplitString(accept_languages_pref, ",", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY)) {
      accept_languages.push_back(lang);
    }
    return accept_languages;
  }

  std::vector<std::string> GetTopLanguages() override {
    // The UrlLanguageHistogram includes the frequency of all languages
    // of pages the user has visited, and some of these might be rare or
    // even mistakes. Set a minimum threshold so that we're only returning
    // languages that account for a nontrivial amount of browsing time.
    // The purpose of this list is to handle the case where users might
    // not be setting their accept languages correctly, we want a way to
    // detect the primary languages a user actually reads.
    const float kMinTopLanguageFrequency = 0.1;

    std::vector<std::string> top_languages;
    language::UrlLanguageHistogram* url_language_histogram =
        UrlLanguageHistogramFactory::GetForBrowserContext(profile_);
    std::vector<LanguageInfo> language_infos =
        url_language_histogram->GetTopLanguages();
    for (const LanguageInfo& info : language_infos) {
      if (info.frequency >= kMinTopLanguageFrequency)
        top_languages.push_back(info.language_code);
    }
    return top_languages;
  }

  void RecordLanguageMetrics(const std::string& page_language,
                             const std::string& requested_language) override {
    base::UmaHistogramSparse(
        "Accessibility.ImageLabels.PageLanguage",
        language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(
            page_language));
    base::UmaHistogramSparse(
        "Accessibility.ImageLabels.RequestLanguage",
        language_usage_metrics::LanguageUsageMetrics::ToLanguageCode(
            requested_language));
  }

 private:
  Profile* const profile_;
  data_decoder::DataDecoder data_decoder_;

  DISALLOW_COPY_AND_ASSIGN(ImageAnnotatorClient);
};

}  // namespace

AccessibilityLabelsService::~AccessibilityLabelsService() {}

// static
void AccessibilityLabelsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsOptInAccepted, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsEnabledAndroid, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsOnlyOnWifi, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
}

// static
void AccessibilityLabelsService::InitOffTheRecordPrefs(
    Profile* off_the_record_profile) {
  DCHECK(off_the_record_profile->IsOffTheRecord());
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsOptInAccepted, false);
#if defined(OS_ANDROID)
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabledAndroid, false);
  off_the_record_profile->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsOnlyOnWifi, true);
#endif
}

void AccessibilityLabelsService::Init() {
  // Hidden behind a feature flag.
  if (!base::FeatureList::IsEnabled(features::kExperimentalAccessibilityLabels))
    return;

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kAccessibilityImageLabelsEnabled,
      base::BindRepeating(
          &AccessibilityLabelsService::OnImageLabelsEnabledChanged,
          weak_factory_.GetWeakPtr()));

  // Log whether the feature is enabled after startup. This must be run on the
  // UI thread because it accesses prefs.
  content::BrowserAccessibilityState::GetInstance()
      ->AddUIThreadHistogramCallback(base::BindOnce(
          &AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms,
          weak_factory_.GetWeakPtr()));
}

AccessibilityLabelsService::AccessibilityLabelsService(Profile* profile)
    : profile_(profile) {}

ui::AXMode AccessibilityLabelsService::GetAXMode() {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();

  // Hidden behind a feature flag.
  if (base::FeatureList::IsEnabled(
          features::kExperimentalAccessibilityLabels)) {
    bool enabled = profile_->GetPrefs()->GetBoolean(
        prefs::kAccessibilityImageLabelsEnabled);
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
  }

  return ax_mode;
}

void AccessibilityLabelsService::EnableLabelsServiceOnce() {
  if (!accessibility_state_utils::IsScreenReaderEnabled()) {
    return;
  }

  // TODO(crbug.com/905419): Implement for Android, which does not support
  // BrowserList::GetInstance.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return;
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  // Fire an AXAction on the active tab to enable this feature once only.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kAnnotatePageImages;
  for (content::RenderFrameHost* frame : web_contents->GetAllFrames()) {
    if (frame->IsRenderFrameLive())
      frame->AccessibilityPerformAction(action_data);
  }
#endif
}

void AccessibilityLabelsService::BindImageAnnotator(
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  if (!remote_service_) {
    auto service_receiver = remote_service_.BindNewPipeAndPassReceiver();
    auto& binder = GetImageAnnotatorBinderOverride();
    if (binder) {
      binder.Run(std::move(service_receiver));
    } else {
      service_ = std::make_unique<image_annotation::ImageAnnotationService>(
          std::move(service_receiver), APIKeyForChannel(),
          profile_->GetURLLoaderFactory(),
          std::make_unique<ImageAnnotatorClient>(profile_));
    }
  }

  remote_service_->BindAnnotator(std::move(receiver));
}

void AccessibilityLabelsService::OverrideImageAnnotatorBinderForTesting(
    ImageAnnotatorBinder binder) {
  GetImageAnnotatorBinderOverride() = std::move(binder);
}

void AccessibilityLabelsService::OnImageLabelsEnabledChanged() {
  // TODO(dmazzoni) Implement for Android, which doesn't support
  // AllTabContentses(). crbug.com/905419
#if !defined(OS_ANDROID)
  bool enabled = profile_->GetPrefs()->GetBoolean(
                     prefs::kAccessibilityImageLabelsEnabled) &&
                 accessibility_state_utils::IsScreenReaderEnabled();

  for (auto* web_contents : AllTabContentses()) {
    if (web_contents->GetBrowserContext() != profile_)
      continue;

    ui::AXMode ax_mode = web_contents->GetAccessibilityMode();
    ax_mode.set_mode(ui::AXMode::kLabelImages, enabled);
    web_contents->SetAccessibilityMode(ax_mode);
  }
#endif
}

void AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms() {
  if (!profile_ || !profile_->GetPrefs())
    return;

  base::UmaHistogramBoolean("Accessibility.ImageLabels",
                            profile_->GetPrefs()->GetBoolean(
                                prefs::kAccessibilityImageLabelsEnabled));
}
