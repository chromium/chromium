// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/autocomplete/tab_matcher_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/android/omnibox/jni_headers/AutocompleteController_jni.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/voice_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using metrics::OmniboxEventProto;

namespace {
// The delay between the Omnibox being opened and a spare renderer being
// started. Starting a spare renderer is a very expensive operation, so this
// value must always be great enough for the Omnibox to be fully rendered and
// otherwise not doing anything important but not so great that the user
// navigates before it occurs. Experimentation between 1s, 2s, 3s found that 1s
// was the most ideal.
static constexpr int OMNIBOX_SPARE_RENDERER_DELAY_MS = 1000;

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

/**
 * A prefetcher class responsible for triggering zero suggest prefetch.
 * The prefetch occurs as a side-effect of calling OnOmniboxFocused() on
 * the AutocompleteController object.
 */
class ZeroSuggestPrefetcher {
 public:
  explicit ZeroSuggestPrefetcher(Profile* profile);

 private:
  void SelfDestruct();

  std::unique_ptr<AutocompleteController> controller_;
  base::OneShotTimer expire_timer_;
};

ZeroSuggestPrefetcher::ZeroSuggestPrefetcher(Profile* profile)
    : controller_(new AutocompleteController(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
  AutocompleteInput input(std::u16string(), metrics::OmniboxEventProto::NTP,
                          ChromeAutocompleteSchemeClassifier(profile));
  input.set_current_url(GURL(chrome::kChromeUINewTabURL));
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);
  controller_->Start(input);
  // Delete ourselves after 10s. This is enough time to cache results or
  // give up if the results haven't been received.
  expire_timer_.Start(FROM_HERE, base::Milliseconds(10000), this,
                      &ZeroSuggestPrefetcher::SelfDestruct);
}

void ZeroSuggestPrefetcher::SelfDestruct() {
  delete this;
}

}  // namespace

AutocompleteControllerAndroid::AutocompleteControllerAndroid(
    Profile* profile,
    std::unique_ptr<ChromeAutocompleteProviderClient> client)
    : profile_{profile},
      java_controller_{Java_AutocompleteController_Constructor(
          AttachCurrentThread(),
          ProfileAndroid::FromProfile(profile)->GetJavaObject(),
          reinterpret_cast<intptr_t>(this))},
      provider_client_{client.get()},
      autocomplete_controller_{std::make_unique<AutocompleteController>(
          std::move(client),
          AutocompleteClassifier::DefaultOmniboxProviders())} {
  autocomplete_controller_->AddObserver(this);

  OmniboxControllerEmitter* emitter =
      OmniboxControllerEmitter::GetForBrowserContext(profile_);
  if (emitter)
    autocomplete_controller_->AddObserver(emitter);
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
  if (!j_current_url.is_null())
    current_url = GURL(ConvertJavaStringToUTF16(env, j_current_url));
  if (!j_desired_tld.is_null())
    desired_tld = base::android::ConvertJavaStringToUTF8(env, j_desired_tld);
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
  input_.set_want_asynchronous_matches(want_asynchronous_matches);
  autocomplete_controller_->Start(input_);
}

void AutocompleteControllerAndroid::StartPrefetch(JNIEnv* env) {
  AutocompleteInput autocomplete_input(
      u"", metrics::OmniboxEventProto::NTP_ZPS_PREFETCH,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  if (base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching)) {
    autocomplete_controller_->StartPrefetch(autocomplete_input);
  } else {
    // ZeroSuggestPrefetcher deletes itself after it's done prefetching.
    new ZeroSuggestPrefetcher(profile_);
  }
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::Classify(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_text,
    bool focused_from_fakebox) {
  // The old AutocompleteResult is about to be invalidated.
  autocomplete_controller_->result().DestroyJavaObject();

  inside_synchronous_start_ = true;
  Start(env, j_text, -1, nullptr, nullptr, true, false, false, false,
        focused_from_fakebox);
  inside_synchronous_start_ = false;
  DCHECK(autocomplete_controller_->done());
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (result.empty())
    return ScopedJavaLocalRef<jobject>();

  return ScopedJavaLocalRef<jobject>(
      result.begin()->GetOrCreateJavaObject(env));
}

void AutocompleteControllerAndroid::OnOmniboxFocused(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_omnibox_text,
    const JavaParamRef<jstring>& j_current_url,
    jint j_page_classification,
    const JavaParamRef<jstring>& j_current_title) {
  // Prevents double triggering of zero suggest when OnOmniboxFocused is issued
  // in quick succession (due to odd timing in the Android focus callbacks).
  if (!autocomplete_controller_->done())
    return;

  std::u16string url = ConvertJavaStringToUTF16(env, j_current_url);
  std::u16string current_title = ConvertJavaStringToUTF16(env, j_current_title);
  const GURL current_url = GURL(url);
  std::u16string omnibox_text = ConvertJavaStringToUTF16(env, j_omnibox_text);

  // If omnibox text is empty, set it to the current URL for the purposes of
  // populating the verbatim match.
  if (omnibox_text.empty() && !current_url.SchemeIs(content::kChromeUIScheme) &&
      !current_url.SchemeIs(browser_ui::kChromeUINativeScheme))
    omnibox_text = url;

  auto page_class =
      OmniboxEventProto::PageClassification(j_page_classification);

  // Proactively start up a renderer, to reduce the time to display search
  // results, especially if a Service Worker is used. This is done in a PostTask
  // with a experiment-configured delay so that the CPU usage associated with
  // starting a new renderer process does not impact the Omnibox initialization.
  // Note that there's a small chance the renderer will be started after the
  // next navigation if the delay is too long, but the spare renderer will
  // probably get used anyways by a later navigation.
  if (!profile_->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(omnibox::kOmniboxSpareRenderer) &&
      page_class != OmniboxEventProto::ANDROID_SEARCH_WIDGET &&
      page_class != OmniboxEventProto::START_SURFACE_HOMEPAGE &&
      page_class != OmniboxEventProto::START_SURFACE_NEW_TAB &&
      !BaseSearchProvider::IsNTPPage(page_class)) {
    auto renderer_delay_ms = base::GetFieldTrialParamByFeatureAsInt(
        omnibox::kOmniboxSpareRenderer, "omnibox_spare_renderer_delay_ms",
        OMNIBOX_SPARE_RENDERER_DELAY_MS);

    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AutocompleteControllerAndroid::WarmUpRenderProcess,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(renderer_delay_ms));
  }

  input_ = AutocompleteInput(omnibox_text, page_class,
                             ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_current_url(current_url);
  input_.set_current_title(current_title);
  input_.set_focus_type(OmniboxFocusType::ON_FOCUS);
  autocomplete_controller_->Start(input_);
}

void AutocompleteControllerAndroid::Stop(JNIEnv* env,
                                         bool clear_results) {
  autocomplete_controller_->Stop(clear_results);
}

void AutocompleteControllerAndroid::ResetSession(JNIEnv* env) {
  autocomplete_controller_->ResetSession();
}

void AutocompleteControllerAndroid::OnSuggestionSelected(
    JNIEnv* env,
    jint match_index,
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

  const auto& match = autocomplete_controller_->result().match_at(match_index);
  SuggestionAnswer::LogAnswerUsed(match.answer);
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          match.destination_url)) {
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.OffTheRecord",
                          profile_->IsOffTheRecord());
  }

  RecordClipboardMetrics(match.type);

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
      input_.focus_type() != OmniboxFocusType::DEFAULT ? std::u16string()
                                                       : input_.text(),
      false,                /* don't know */
      input_.type(), false, /* not keyword mode */
      OmniboxEventProto::INVALID, true, match_index,
      static_cast<WindowOpenDisposition>(j_window_open_disposition), false,
      sessions::SessionTabHelper::IdForTab(web_contents),
      OmniboxEventProto::PageClassification(j_page_classification),
      base::Milliseconds(elapsed_time_since_first_modified), completed_length,
      now - autocomplete_controller_->last_time_default_match_changed(),
      autocomplete_controller_->result());
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  // Record the value if prerender for search suggestion was not started. Other
  // values (kHitFinished, kUnused, kCancelled) are recorded in
  // PrerenderManager.
  if (web_contents) {
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

void AutocompleteControllerAndroid::DeleteMatch(JNIEnv* env, jint match_index) {
  const auto& match = autocomplete_controller_->result().match_at(match_index);
  if (match.SupportsDeletion())
    autocomplete_controller_->DeleteMatch(match);
}

void AutocompleteControllerAndroid::DeleteMatchElement(JNIEnv* env,
                                                       jint match_index,
                                                       jint element_index) {
  const auto& match = autocomplete_controller_->result().match_at(match_index);
  if (match.SupportsDeletion())
    autocomplete_controller_->DeleteMatchElement(match, element_index);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::
    UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
        JNIEnv* env,
        jint match_index,
        jlong elapsed_time_since_input_change,
        const JavaParamRef<jstring>& jnew_query_text,
        const JavaParamRef<jobjectArray>& jnew_query_params) {
  AutocompleteMatch match(
      autocomplete_controller_->result().match_at(match_index));

  if (!jnew_query_text.is_null()) {
    std::u16string query =
        base::android::ConvertJavaStringToUTF16(env, jnew_query_text);
    if (!match.search_terms_args) {
      match.search_terms_args =
          std::make_unique<TemplateURLRef::SearchTermsArgs>(query);
    } else {
      match.search_terms_args->search_terms = query;
    }
  }

  if (!jnew_query_params.is_null() && match.search_terms_args) {
    std::vector<std::string> params;
    base::android::AppendJavaStringArrayToStringVector(env, jnew_query_params,
                                                       &params);
    // The query params are from the query tiles server and doesn't need to be
    // escaped.
    match.search_terms_args->additional_query_params =
        base::JoinString(params, "&");
  }
  autocomplete_controller_
      ->UpdateMatchDestinationURLWithAdditionalAssistedQueryStats(
          base::Milliseconds(elapsed_time_since_input_change), &match);
  return url::GURLAndroid::FromNativeGURL(env, match.destination_url);
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetMatchingTabForSuggestion(JNIEnv* env,
                                                           jint match_index) {
  const AutocompleteMatch& match =
      autocomplete_controller_->result().match_at(match_index);
  return match.GetMatchingJavaTab().get(env);
}

void AutocompleteControllerAndroid::Shutdown() {
  // Cancel all pending actions and clear any remaining matches.
  autocomplete_controller_.reset();
  Java_AutocompleteController_notifyNativeDestroyed(AttachCurrentThread(),
                                                    java_controller_);
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

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::GetJavaObject()
    const {
  return ScopedJavaLocalRef<jobject>(java_controller_);
}

AutocompleteControllerAndroid::~AutocompleteControllerAndroid() = default;

void AutocompleteControllerAndroid::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  if (!inside_synchronous_start_)
    NotifySuggestionsReceived(autocomplete_controller_->result());
}

void AutocompleteControllerAndroid::NotifySuggestionsReceived(
    const AutocompleteResult& autocomplete_result) {
  JNIEnv* env = AttachCurrentThread();

  autocomplete_controller_->SetTailSuggestContentPrefixes();

  // Get the inline-autocomplete text.
  std::u16string inline_autocompletion;
  if (auto* default_match = autocomplete_result.default_match())
    inline_autocompletion = default_match->inline_autocompletion;
  ScopedJavaLocalRef<jstring> inline_text =
      ConvertUTF16ToJavaString(env, inline_autocompletion);

  Java_AutocompleteController_onSuggestionsReceived(
      env, java_controller_, autocomplete_result.GetOrCreateJavaObject(env),
      inline_text);
}

void AutocompleteControllerAndroid::WarmUpRenderProcess() const {
  // It is ok for this to get called multiple times since all the requests
  // will get de-duplicated to the first one.
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile_);
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

content::BrowserContext*
AutocompleteControllerAndroid::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

AutocompleteControllerAndroid::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AutocompleteControllerAndroid",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(ShortcutsBackendFactory::GetInstance());
}

AutocompleteControllerAndroid::Factory::~Factory() = default;

KeyedService* AutocompleteControllerAndroid::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = static_cast<Profile*>(context);
  return new AutocompleteControllerAndroid(
      profile, std::make_unique<ChromeAutocompleteProviderClient>(profile));
}

static ScopedJavaLocalRef<jobject> JNI_AutocompleteController_GetForProfile(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  AutocompleteControllerAndroid* native_bridge =
      AutocompleteControllerAndroid::Factory::GetForProfile(
          ProfileAndroid::FromProfileAndroid(jprofile));
  if (!native_bridge)
    return {};
  return native_bridge->GetJavaObject();
}
