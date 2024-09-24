// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/autocomplete/tab_matcher_android.h"
#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/omnibox/browser/actions/omnibox_answer_action.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"
#include "components/omnibox/browser/autocomplete_grouper_sections.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_fuzzy_provider.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/voice_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/omnibox/jni_headers/AutocompleteController_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaFloatArrayToFloatVector;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using metrics::OmniboxEventProto;

namespace {

void RecordClipboardMetrics(AutocompleteMatchType::Type match_type) {
  if (match_type != AutocompleteMatchType::CLIPBOARD_URL &&
      match_type != AutocompleteMatchType::CLIPBOARD_TEXT &&
      match_type != AutocompleteMatchType::CLIPBOARD_IMAGE) {
    return;
  }

  base::TimeDelta age =
      ClipboardRecentContent::GetInstance()->GetClipboardContentAge();
  UMA_HISTOGRAM_LONG_TIMES_100("MobileOmnibox.PressedClipboardSuggestionAge",
                               age);
  if (match_type == AutocompleteMatchType::CLIPBOARD_URL) {
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge.URL", age);
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge.TEXT", age);
  } else if (match_type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge.IMAGE", age);
  }
}

}  // namespace

AutocompleteControllerAndroid::AutocompleteControllerAndroid(
    Profile* profile,
    std::unique_ptr<ChromeAutocompleteProviderClient> client,
    bool is_low_memory_device)
    : profile_{profile},
      java_controller_{Java_AutocompleteController_Constructor(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))},
      autocomplete_controller_{std::make_unique<AutocompleteController>(
          std::move(client),
          AutocompleteClassifier::DefaultOmniboxProviders(
              is_low_memory_device))} {
  autocomplete_controller_->AddObserver(this);

  AutocompleteControllerEmitter* emitter =
      AutocompleteControllerEmitterFactory::GetForBrowserContext(profile_);
  if (emitter) {
    autocomplete_controller_->AddObserver(emitter);
  }
}

void AutocompleteControllerAndroid::Start(JNIEnv* env,
                                          const JavaRef<jstring>& j_text,
                                          jint j_cursor_pos,
                                          const JavaRef<jstring>& j_desired_tld,
                                          const JavaRef<jstring>& j_current_url,
                                          jint j_page_classification,
                                          bool prevent_inline_autocomplete,
                                          bool prefer_keyword,
                                          bool allow_exact_keyword_match,
                                          bool want_asynchronous_matches) {
  autocomplete_controller_->result().DestroyJavaObject();

  std::string desired_tld;
  GURL current_url;
  if (!j_current_url.is_null()) {
    current_url = GURL(ConvertJavaStringToUTF16(env, j_current_url));
  }
  if (!j_desired_tld.is_null()) {
    desired_tld = ConvertJavaStringToUTF8(env, j_desired_tld);
  }
  std::u16string text = ConvertJavaStringToUTF16(env, j_text);
  size_t cursor_pos = j_cursor_pos == -1 ? std::u16string::npos : j_cursor_pos;
  input_ = AutocompleteInput(
      text, cursor_pos, desired_tld,
      OmniboxEventProto::PageClassification(j_page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_current_url(current_url);
  input_.set_prevent_inline_autocomplete(prevent_inline_autocomplete);
  input_.set_prefer_keyword(prefer_keyword);
  input_.set_allow_exact_keyword_match(allow_exact_keyword_match);
  input_.set_omit_asynchronous_matches(!want_asynchronous_matches);
  autocomplete_controller_->Start(input_);
}

void AutocompleteControllerAndroid::StartPrefetch(
    JNIEnv* env,
    const JavaRef<jstring>& j_current_url,
    jint j_page_classification) {
  auto page_classification =
      OmniboxEventProto::PageClassification(j_page_classification);
  if (!OmniboxFieldTrial::IsZeroSuggestPrefetchingEnabledInContext(
          page_classification)) {
    return;
  }

  const bool is_ntp_page = omnibox::IsNTPPage(page_classification);
  const bool interaction_clobber_focus_type =
      base::FeatureList::IsEnabled(
          omnibox::kOmniboxOnClobberFocusTypeOnContent) &&
      !is_ntp_page;

  GURL current_url;
  std::u16string auto_complete_text;

  if (!j_current_url.is_null()) {
    current_url = GURL(ConvertJavaStringToUTF16(env, j_current_url));

    // We will not assign text to autocomplete input when on NTP page and input
    // type is not clobber focus type.
    if (!is_ntp_page && !interaction_clobber_focus_type) {
      auto_complete_text = ConvertJavaStringToUTF16(env, j_current_url);
    }
  }

  AutocompleteInput input(auto_complete_text, page_classification,
                          ChromeAutocompleteSchemeClassifier(profile_));
  input.set_current_url(current_url);
  input.set_focus_type(interaction_clobber_focus_type
                           ? metrics::OmniboxFocusType::INTERACTION_CLOBBER
                           : metrics::OmniboxFocusType::INTERACTION_FOCUS);
  autocomplete_controller_->StartPrefetch(input);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::Classify(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_text) {
  // The old AutocompleteResult is about to be invalidated.
  autocomplete_controller_->result().DestroyJavaObject();

  inside_synchronous_start_ = true;
  Start(env, j_text, -1, nullptr, nullptr, true, false, false, false, false);
  inside_synchronous_start_ = false;
  DCHECK(autocomplete_controller_->done());
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (result.empty()) {
    return ScopedJavaLocalRef<jobject>();
  }

  return ScopedJavaLocalRef<jobject>(
      result.begin()->GetOrCreateJavaObject(env));
}

void AutocompleteControllerAndroid::OnOmniboxFocused(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_omnibox_text,
    const JavaParamRef<jstring>& j_current_url,
    jint j_page_classification,
    const JavaParamRef<jstring>& j_current_title,
    bool is_on_focus_context) {
  // Prevents double triggering of zero suggest when OnOmniboxFocused is issued
  // in quick succession (due to odd timing in the Android focus callbacks).
  if (!autocomplete_controller_->done()) {
    return;
  }

  std::u16string url = ConvertJavaStringToUTF16(env, j_current_url);
  std::u16string current_title = ConvertJavaStringToUTF16(env, j_current_title);
  const GURL current_url = GURL(url);
  std::u16string omnibox_text = ConvertJavaStringToUTF16(env, j_omnibox_text);

  // If omnibox text is empty, set it to the current URL for the purposes of
  // populating the verbatim match.
  if (omnibox_text.empty() && !current_url.SchemeIs(content::kChromeUIScheme) &&
      !current_url.SchemeIs(browser_ui::kChromeUINativeScheme)) {
    omnibox_text = url;
  }

  auto page_class =
      OmniboxEventProto::PageClassification(j_page_classification);
  const bool interaction_clobber_focus_type =
      !(omnibox::IsNTPPage(page_class) ||
        (is_on_focus_context &&
         base::FeatureList::IsEnabled(omnibox::kRetainOmniboxOnFocus)));
  if (interaction_clobber_focus_type) {
    omnibox_text.clear();
  }

  // Proactively start up a renderer, to reduce the time to display search
  // results, especially if a Service Worker is used. This is done in a PostTask
  // with a experiment-configured delay so that the CPU usage associated with
  // starting a new renderer process does not impact the Omnibox initialization.
  // Note that there's a small chance the renderer will be started after the
  // next navigation if the delay is too long, but the spare renderer will
  // probably get used anyways by a later navigation.
  if (!profile_->IsOffTheRecord() &&
      page_class != OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      !omnibox::IsNTPPage(page_class)) {
    int spare_renderer_delay_ms = omnibox::kOmniboxSpareRendererDelayMs.Get();
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutocompleteControllerAndroid::WarmUpRenderProcess,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(spare_renderer_delay_ms));
  }

  input_ = AutocompleteInput(omnibox_text, page_class,
                             ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_current_url(current_url);
  input_.set_current_title(current_title);

  // Assign focus type to INTERACTION_CLOBBER to non-NTP zero-prefix requests
  input_.set_focus_type(interaction_clobber_focus_type
                            ? metrics::OmniboxFocusType::INTERACTION_CLOBBER
                            : metrics::OmniboxFocusType::INTERACTION_FOCUS);

  autocomplete_controller_->Start(input_);
}

void AutocompleteControllerAndroid::Stop(JNIEnv* env, bool clear_results) {
  autocomplete_controller_->Stop(clear_results);
}

void AutocompleteControllerAndroid::ResetSession(JNIEnv* env) {
  autocomplete_controller_->ResetSession();
}

void AutocompleteControllerAndroid::OnSuggestionSelected(
    JNIEnv* env,
    uintptr_t match_ptr,
    int suggestion_line,
    const jint j_window_open_disposition,
    const JavaParamRef<jstring>& j_current_url,
    jint j_page_classification,
    jlong elapsed_time_since_first_modified,
    jint completed_length,
    const JavaParamRef<jobject>& j_web_contents) {
  std::u16string url = ConvertJavaStringToUTF16(env, j_current_url);
  const GURL current_url = GURL(url);
  const base::TimeTicks& now(base::TimeTicks::Now());
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  const auto& match = *reinterpret_cast<AutocompleteMatch*>(match_ptr);
  omnibox::answer_data_parser::LogAnswerUsed(match.answer_type);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.OffTheRecord",
                          profile_->IsOffTheRecord());
  }

  RecordClipboardMetrics(match.type);
  HistoryFuzzyProvider::RecordOpenMatchMetrics(
      autocomplete_controller_->result(), match);

  // The following histogram should be recorded for both TYPED and pasted
  // URLs, but should still exclude reloads.
  if (ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_TYPED) ||
      ui::PageTransitionTypeIncludingQualifiersIs(match.transition,
                                                  ui::PAGE_TRANSITION_LINK)) {
    net::cookie_util::RecordCookiePortOmniboxHistograms(match.destination_url);
  }

  AutocompleteMatch::LogSearchEngineUsed(
      match, TemplateURLServiceFactory::GetForProfile(profile_));

  OmniboxLog log(
      // For zero suggest, record an empty input string instead of the
      // current URL.
      input_.IsZeroSuggest() ? std::u16string() : input_.text(),
      false,                /* don't know */
      input_.type(), false, /* not keyword mode */
      OmniboxEventProto::INVALID, true, OmniboxPopupSelection(suggestion_line),
      static_cast<WindowOpenDisposition>(j_window_open_disposition), false,
      sessions::SessionTabHelper::IdForTab(web_contents),
      OmniboxEventProto::PageClassification(j_page_classification),
      base::Milliseconds(elapsed_time_since_first_modified), completed_length,
      now - autocomplete_controller_->last_time_default_match_changed(),
      autocomplete_controller_->result(), match.destination_url,
      profile_->IsOffTheRecord());
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  if (web_contents) {
    if (auto* search_prefetch_service =
            SearchPrefetchServiceFactory::GetForProfile(profile_)) {
      search_prefetch_service->OnURLOpenedFromOmnibox(&log);
    }

    // Record the value if prerender for search suggestion was not started.
    // Other values (kHitFinished, kUnused, kCancelled) are recorded in
    // PrerenderManager.
    auto* prerender_manager = PrerenderManager::FromWebContents(web_contents);
    if (!prerender_manager ||
        !prerender_manager->HasSearchResultPagePrerendered()) {
      base::UmaHistogramEnumeration(
          internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
          PrerenderPredictionStatus::kNotStarted);
    }
  }
  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(log);
}

jboolean AutocompleteControllerAndroid::OnSuggestionTouchDown(
    JNIEnv* env,
    uintptr_t match_ptr,
    int match_index,
    const base::android::JavaParamRef<jobject>& j_web_contents) {
  const auto& match = *reinterpret_cast<AutocompleteMatch*>(match_ptr);

  if (SearchPrefetchService* search_prefetch_service =
          SearchPrefetchServiceFactory::GetForProfile(profile_)) {
    return search_prefetch_service->OnNavigationLikely(
        match_index, match, omnibox::mojom::NavigationPredictor::kTouchDown,
        content::WebContents::FromJavaWebContents(j_web_contents));
  }
  return false;
}

void AutocompleteControllerAndroid::DeleteMatch(JNIEnv* env,
                                                uintptr_t match_ptr) {
  const auto* match = reinterpret_cast<AutocompleteMatch*>(match_ptr);
  if (match->SupportsDeletion()) {
    autocomplete_controller_->DeleteMatch(*match);
  }
}

void AutocompleteControllerAndroid::DeleteMatchElement(JNIEnv* env,
                                                       uintptr_t match_ptr,
                                                       jint element_index) {
  const auto* match = reinterpret_cast<AutocompleteMatch*>(match_ptr);
  if (match->SupportsDeletion()) {
    autocomplete_controller_->DeleteMatchElement(*match, element_index);
  }
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::
    UpdateMatchDestinationURLWithAdditionalSearchboxStats(
        JNIEnv* env,
        uintptr_t match_ptr,
        jlong elapsed_time_since_input_change) {
  auto* match = reinterpret_cast<AutocompleteMatch*>(match_ptr);
  autocomplete_controller_
      ->UpdateMatchDestinationURLWithAdditionalSearchboxStats(
          base::Milliseconds(elapsed_time_since_input_change), match);
  return url::GURLAndroid::FromNativeGURL(env, match->destination_url);
}

base::android::ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetAnswerActionDestinationURL(
    JNIEnv* env,
    uintptr_t match_ptr,
    jlong elapsed_time_since_input_change,
    uintptr_t answer_action_ptr) {
  auto* match = reinterpret_cast<AutocompleteMatch*>(match_ptr);
  auto* action = reinterpret_cast<OmniboxAnswerAction*>(answer_action_ptr);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);

  if (action == nullptr || template_url_service == nullptr) {
    return url::GURLAndroid::FromNativeGURL(env, GURL());
  }

  autocomplete_controller_->UpdateSearchTermsArgsWithAdditionalSearchboxStats(
      base::Milliseconds(elapsed_time_since_input_change),
      action->search_terms_args);
  TemplateURL* template_url =
      match->GetTemplateURL(template_url_service, false);
  return url::GURLAndroid::FromNativeGURL(
      env, GURL(template_url->url_ref().ReplaceSearchTerms(
               action->search_terms_args,
               template_url_service->search_terms_data())));
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetMatchingTabForSuggestion(
    JNIEnv* env,
    uintptr_t match_ptr) {
  const auto& match = *reinterpret_cast<AutocompleteMatch*>(match_ptr);
  return match.GetMatchingJavaTab().get(env);
}

void AutocompleteControllerAndroid::Shutdown() {
  // Cancel all pending actions and clear any remaining matches.
  autocomplete_controller_.reset();
  Java_AutocompleteController_notifyNativeDestroyed(AttachCurrentThread(),
                                                    java_controller_);
}

// static
void AutocompleteControllerAndroid::EnsureFactoryBuilt() {
  AutocompleteControllerAndroid::Factory::GetInstance();
}

void AutocompleteControllerAndroid::SetVoiceMatches(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_voice_matches,
    const JavaParamRef<jfloatArray>& j_confidence_scores) {
  auto* const voice_suggest_provider =
      autocomplete_controller_->voice_suggest_provider();
  DCHECK(voice_suggest_provider)
      << "Voice matches received with no registered VoiceSuggestProvider. "
      << "Either disable voice input, or provision VoiceSuggestProvider.";

  std::vector<std::u16string> voice_matches;
  std::vector<float> confidence_scores;
  AppendJavaStringArrayToStringVector(env, j_voice_matches, &voice_matches);
  JavaFloatArrayToFloatVector(env, j_confidence_scores, &confidence_scores);
  DCHECK(voice_matches.size() == confidence_scores.size());

  voice_suggest_provider->ClearCache();
  for (size_t index = 0; index < voice_matches.size(); ++index) {
    voice_suggest_provider->AddVoiceSuggestion(voice_matches[index],
                                               confidence_scores[index]);
  }
}

void AutocompleteControllerAndroid::OnSuggestionDropdownHeightChanged(
    JNIEnv* env,
    jint dropdown_height_with_keyboard_active_px,
    jint suggestion_height_px) {
  if (suggestion_height_px == 0) {
    // Don't touch the group definitions.
    return;
  }

  size_t num_visible_matches =
      (size_t)(1.f * dropdown_height_with_keyboard_active_px /
                   suggestion_height_px +
               0.5f);

  if (num_visible_matches == 0) {
    return;
  }

  AndroidNonZPSSection::set_num_visible_matches(num_visible_matches);
}

void AutocompleteControllerAndroid::CreateNavigationObserver(
    JNIEnv* env,
    uintptr_t navigation_handle_ptr,
    uintptr_t match_ptr) {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxShortcutsAndroid)) {
    return;
  }

  auto* navigation_handle =
      reinterpret_cast<content::NavigationHandle*>(navigation_handle_ptr);
  const auto& match = *reinterpret_cast<AutocompleteMatch*>(match_ptr);

  ChromeOmniboxNavigationObserverAndroid::Create(navigation_handle, profile_,
                                                 input_.text(), match);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::GetJavaObject()
    const {
  return ScopedJavaLocalRef<jobject>(java_controller_);
}

AutocompleteControllerAndroid::~AutocompleteControllerAndroid() = default;

void AutocompleteControllerAndroid::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  if (!inside_synchronous_start_) {
    NotifySuggestionsReceived(autocomplete_controller_->result());
  }
}

void AutocompleteControllerAndroid::NotifySuggestionsReceived(
    const AutocompleteResult& autocomplete_result) {
  JNIEnv* env = AttachCurrentThread();

  Java_AutocompleteController_onSuggestionsReceived(
      env, java_controller_, autocomplete_result.GetOrCreateJavaObject(env),
      autocomplete_controller_->done());
}

void AutocompleteControllerAndroid::WarmUpRenderProcess() const {
  // It is ok for this to get called multiple times since all the requests
  // will get de-duplicated to the first one.
  content::SpareRenderProcessHostManager::Get().WarmupSpare(profile_);
}

// static
AutocompleteControllerAndroid*
AutocompleteControllerAndroid::Factory::GetForProfile(Profile* profile) {
  return static_cast<AutocompleteControllerAndroid*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AutocompleteControllerAndroid::Factory*
AutocompleteControllerAndroid::Factory::GetInstance() {
  return base::Singleton<AutocompleteControllerAndroid::Factory>::get();
}

AutocompleteControllerAndroid::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "AutocompleteControllerAndroid",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(ShortcutsBackendFactory::GetInstance());
}

AutocompleteControllerAndroid::Factory::~Factory() = default;

KeyedService* AutocompleteControllerAndroid::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = static_cast<Profile*>(context);
  return new AutocompleteControllerAndroid(
      profile, std::make_unique<ChromeAutocompleteProviderClient>(profile),
      false);
}

static ScopedJavaLocalRef<jobject> JNI_AutocompleteController_GetForProfile(
    JNIEnv* env,
    Profile* profile) {
  AutocompleteControllerAndroid* native_bridge =
      AutocompleteControllerAndroid::Factory::GetForProfile(profile);
  if (!native_bridge) {
    return {};
  }
  return native_bridge->GetJavaObject();
}
