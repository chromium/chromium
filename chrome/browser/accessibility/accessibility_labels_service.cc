// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_labels_service.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/language_usage_metrics.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "google_apis/google_api_keys.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/image_annotation/image_annotation_service.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#else
#include "base/android/jni_android.h"
#include "chrome/browser/image_descriptions/jni_headers/ImageDescriptionsController_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/platform/ax_platform_node.h"
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

  ImageAnnotatorClient(const ImageAnnotatorClient&) = delete;
  ImageAnnotatorClient& operator=(const ImageAnnotatorClient&) = delete;

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
        language::LanguageUsageMetrics::ToLanguageCodeHash(page_language));
    base::UmaHistogramSparse(
        "Accessibility.ImageLabels.RequestLanguage",
        language::LanguageUsageMetrics::ToLanguageCodeHash(requested_language));
  }

 private:
  const raw_ptr<Profile> profile_;
  data_decoder::DataDecoder data_decoder_;
};

}  // namespace

#if !BUILDFLAG(IS_ANDROID)
AccessibilityLabelsService::AccessibilityLabelsService(Profile* profile)
    : profile_(profile) {}
AccessibilityLabelsService::~AccessibilityLabelsService() = default;
#else
// On Android we must add/remove a NetworkChangeObserver during construction/
// destruction to provide the "Only on Wi-Fi" functionality.
// We also add/remove an AXModeObserver to track users enabling a screenreader.
AccessibilityLabelsService::AccessibilityLabelsService(Profile* profile)
    : profile_(profile) {
  // Ensure the |BrowserAccessibilityState| is constructed before adding any
  // observers. The |BrowserAccessibilityState| may change the accessibility
  // mode in its constructor, so if we register the observer before the
  // constructor, we will get a crash.
  auto* state = content::BrowserAccessibilityState::GetInstance();
  DCHECK(state);

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  ui::AXPlatformNode::AddAXModeObserver(this);
}
AccessibilityLabelsService::~AccessibilityLabelsService() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}
#endif

// static
void AccessibilityLabelsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityImageLabelsOptInAccepted, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(IS_ANDROID)
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
}

void AccessibilityLabelsService::Init() {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
#if !BUILDFLAG(IS_ANDROID)
      prefs::kAccessibilityImageLabelsEnabled,
#else
      prefs::kAccessibilityImageLabelsEnabledAndroid,
#endif
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

ui::AXMode AccessibilityLabelsService::GetAXMode() {
  ui::AXMode ax_mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();

#if !BUILDFLAG(IS_ANDROID)
  ax_mode.set_mode(ui::AXMode::kLabelImages,
                   profile_->GetPrefs()->GetBoolean(
                       prefs::kAccessibilityImageLabelsEnabled));
#else
  ax_mode.set_mode(ui::AXMode::kLabelImages, GetAndroidEnabledStatus());
#endif

  return ax_mode;
}

void AccessibilityLabelsService::EnableLabelsServiceOnce() {
  if (!accessibility_state_utils::IsScreenReaderEnabled()) {
    return;
  }

  // For Android, we call through the JNI (see below) and send the web contents
  // directly, since Android does not support BrowserList::GetInstance.
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return;
  auto* web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;
  // Fire an AXAction on the active tab to enable this feature once only.
  // We only need to fire this event for the active page.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kAnnotatePageImages;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&action_data](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive()) {
          render_frame_host->AccessibilityPerformAction(action_data);
        }
      });
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
#if !BUILDFLAG(IS_ANDROID)
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
#else
  // Android does not support AllTabContentses(), so we will get all web
  // contents from the state and set the new AXMode there.
  content::BrowserAccessibilityState::GetInstance()
      ->SetImageLabelsModeForProfile(GetAndroidEnabledStatus(), profile_);
#endif
}

void AccessibilityLabelsService::UpdateAccessibilityLabelsHistograms() {
  if (!profile_ || !profile_->GetPrefs())
    return;

  base::UmaHistogramBoolean("Accessibility.ImageLabels2",
                            profile_->GetPrefs()->GetBoolean(
                                prefs::kAccessibilityImageLabelsEnabled));

#if BUILDFLAG(IS_ANDROID)
  // For Android we will track additional histograms.
  base::UmaHistogramBoolean(
      "Accessibility.ImageLabels.Android",
      profile_->GetPrefs()->GetBoolean(
          prefs::kAccessibilityImageLabelsEnabledAndroid));

  base::UmaHistogramBoolean("Accessibility.ImageLabels.Android.OnlyOnWifi",
                            profile_->GetPrefs()->GetBoolean(
                                prefs::kAccessibilityImageLabelsOnlyOnWifi));
#endif
}

#if BUILDFLAG(IS_ANDROID)
void AccessibilityLabelsService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // When the network status changes, we want to (potentially) update the
  // AXMode of all web contents for the current profile.
  content::BrowserAccessibilityState::GetInstance()
      ->SetImageLabelsModeForProfile(GetAndroidEnabledStatus(), profile_);
}

void AccessibilityLabelsService::OnAXModeAdded(ui::AXMode mode) {
  // When the AXMode changes (e.g. user turned on a screenreader), we want to
  // (potentially) update the AXMode of all web contents for current profile.
  content::BrowserAccessibilityState::GetInstance()
      ->SetImageLabelsModeForProfile(GetAndroidEnabledStatus(), profile_);
}

bool AccessibilityLabelsService::GetAndroidEnabledStatus() {
  // On Android, user has an option to toggle "only on wifi", so also check
  // the current connection type if necessary.
  bool enabled = profile_->GetPrefs()->GetBoolean(
                     prefs::kAccessibilityImageLabelsEnabledAndroid) &&
                 accessibility_state_utils::IsScreenReaderEnabled();

  bool only_on_wifi = profile_->GetPrefs()->GetBoolean(
      prefs::kAccessibilityImageLabelsOnlyOnWifi);

  if (enabled && only_on_wifi) {
    enabled = net::NetworkChangeNotifier::GetConnectionType() ==
              net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI;
  }

  return enabled;
}

void JNI_ImageDescriptionsController_GetImageDescriptionsOnce(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  if (!web_contents)
    return;

  // We only need to fire this event for the active page.
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kAnnotatePageImages;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&action_data](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive()) {
          render_frame_host->AccessibilityPerformAction(action_data);
        }
      });
}
#endif
