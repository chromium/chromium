// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/android/chrome_jni_headers/AutocompleteController_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_controller_emitter.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/voice_suggest_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/android/tile_conversion_bridge.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/tile_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/device_form_factor.h"
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
using base::android::ToJavaByteArray;
using base::android::ToJavaIntArray;
using bookmarks::BookmarkModel;
using metrics::OmniboxEventProto;

namespace {

// Used for histograms, append only.
enum class MatchValidationResult {
  VALID_MATCH = 0,
  WRONG_MATCH = 1,
  BAD_RESULT_SIZE = 2,
  COUNT = 3
};

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
  expire_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10000), this,
                      &ZeroSuggestPrefetcher::SelfDestruct);
}

void ZeroSuggestPrefetcher::SelfDestruct() {
  delete this;
}

}  // namespace

AutocompleteControllerAndroid::AutocompleteControllerAndroid(Profile* profile) {
  std::unique_ptr<ChromeAutocompleteProviderClient> provider_client =
      std::make_unique<ChromeAutocompleteProviderClient>(profile);
  provider_client_ = provider_client.get();
  autocomplete_controller_ = std::make_unique<AutocompleteController>(
      std::move(provider_client),
      AutocompleteClassifier::DefaultOmniboxProviders());
  inside_synchronous_start_ = false;
  profile_ = profile;
  autocomplete_controller_->AddObserver(this);

  OmniboxControllerEmitter* emitter =
      OmniboxControllerEmitter::GetForBrowserContext(profile_);
  if (emitter)
    autocomplete_controller_->AddObserver(emitter);
}

void AutocompleteControllerAndroid::Start(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& j_text,
    jint j_cursor_pos,
    const JavaRef<jstring>& j_desired_tld,
    const JavaRef<jstring>& j_current_url,
    jint j_page_classification,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    bool want_asynchronous_matches,
    const JavaRef<jstring>& j_query_tile_id,
    bool is_query_started_from_tiles) {
  if (!autocomplete_controller_)
    return;

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
  if (!j_query_tile_id.is_null()) {
    std::string tile_id = ConvertJavaStringToUTF8(env, j_query_tile_id);
    input_.set_query_tile_id(tile_id);
    if (base::FeatureList::IsEnabled(
            query_tiles::features::kQueryTilesLocalOrdering)) {
      provider_client_->GetQueryTileService()->OnTileClicked(tile_id);
    }
  }
  is_query_started_from_tiles_ = is_query_started_from_tiles;
  autocomplete_controller_->Start(input_);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::Classify(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_text,
    bool focused_from_fakebox) {
  if (!autocomplete_controller_)
    return ScopedJavaLocalRef<jobject>();

  inside_synchronous_start_ = true;
  Start(env, obj, j_text, -1, nullptr, nullptr, true, false, false, false,
        focused_from_fakebox, JavaRef<jstring>(), false);
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
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_omnibox_text,
    const JavaParamRef<jstring>& j_current_url,
    jint j_page_classification,
    const JavaParamRef<jstring>& j_current_title) {
  if (!autocomplete_controller_)
    return;

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

  input_ = AutocompleteInput(
      omnibox_text,
      OmniboxEventProto::PageClassification(j_page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_current_url(current_url);
  input_.set_current_title(current_title);
  input_.set_focus_type(OmniboxFocusType::ON_FOCUS);
  is_query_started_from_tiles_ = false;
  autocomplete_controller_->Start(input_);
}

void AutocompleteControllerAndroid::Stop(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         bool clear_results) {
  if (autocomplete_controller_ != nullptr)
    autocomplete_controller_->Stop(clear_results);
}

void AutocompleteControllerAndroid::ResetSession(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (autocomplete_controller_ != nullptr)
    autocomplete_controller_->ResetSession();
}

void AutocompleteControllerAndroid::OnSuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint selected_index,
    const jint j_window_open_disposition,
    jint hash_code,
    const JavaParamRef<jstring>& j_current_url,
    jint j_page_classification,
    jlong elapsed_time_since_first_modified,
    jint completed_length,
    const JavaParamRef<jobject>& j_web_contents) {
  if (!IsValidMatch(env, selected_index, hash_code))
    return;

  std::u16string url = ConvertJavaStringToUTF16(env, j_current_url);
  const GURL current_url = GURL(url);
  const base::TimeTicks& now(base::TimeTicks::Now());
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  const auto& match =
      autocomplete_controller_->result().match_at(selected_index);
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
      OmniboxEventProto::INVALID, true, selected_index,
      static_cast<WindowOpenDisposition>(j_window_open_disposition), false,
      sessions::SessionTabHelper::IdForTab(web_contents),
      OmniboxEventProto::PageClassification(j_page_classification),
      base::TimeDelta::FromMilliseconds(elapsed_time_since_first_modified),
      completed_length,
      now - autocomplete_controller_->last_time_default_match_changed(),
      autocomplete_controller_->result());
  log.is_query_started_from_tile = is_query_started_from_tiles_;
  autocomplete_controller_->AddProviderAndTriggeringLogs(&log);

  OmniboxEventGlobalTracker::GetInstance()->OnURLOpened(&log);

  predictors::AutocompleteActionPredictorFactory::GetForProfile(profile_)
      ->OnOmniboxOpenedUrl(log);
}

void AutocompleteControllerAndroid::DeleteSuggestion(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint selected_index,
    jint hash_code) {
  if (!IsValidMatch(env, selected_index, hash_code))
    return;

  const AutocompleteResult& result = autocomplete_controller_->result();
  const AutocompleteMatch& match = result.match_at(selected_index);
  if (match.SupportsDeletion())
    autocomplete_controller_->DeleteMatch(match);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::
    UpdateMatchDestinationURLWithQueryFormulationTime(
        JNIEnv* env,
        const JavaParamRef<jobject>& obj,
        jint selected_index,
        jint hash_code,
        jlong elapsed_time_since_input_change,
        const base::android::JavaParamRef<jstring>& jnew_query_text,
        const base::android::JavaParamRef<jobjectArray>& jnew_query_params) {
  if (!IsValidMatch(env, selected_index, hash_code))
    return ScopedJavaLocalRef<jstring>();
  AutocompleteMatch match(
      autocomplete_controller_->result().match_at(selected_index));

  if (!jnew_query_text.is_null()) {
    std::u16string query =
        base::android::ConvertJavaStringToUTF16(env, jnew_query_text);
    if (!match.search_terms_args) {
      match.search_terms_args.reset(new TemplateURLRef::SearchTermsArgs(query));
    } else {
      match.search_terms_args->search_terms = query;
    }
    if (match.type == AutocompleteMatchType::TILE_SUGGESTION &&
        base::FeatureList::IsEnabled(
            query_tiles::features::kQueryTilesLocalOrdering)) {
      // If the search is from clicking on a tile, report the click
      // so that we can adjust the ordering of the tiles later.
      // Because we don't have tile Id here, pass parent tile's Id
      // and the full query string to TileService to locate the Id.
      // In future, we could simplify this by passing the last tile
      // Id to native.
      provider_client_->GetQueryTileService()->OnQuerySelected(
          input_.query_tile_id(), query);
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
  autocomplete_controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      base::TimeDelta::FromMilliseconds(elapsed_time_since_input_change),
      &match);
  return url::GURLAndroid::FromNativeGURL(env, match.destination_url);
}

base::android::ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::FindMatchingTabWithUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl) {
  TabAndroid* tab = provider_client_->GetTabOpenWithURL(
      *url::GURLAndroid::ToNativeGURL(env, j_gurl), nullptr);

  return tab ? tab->GetJavaObject() : nullptr;
}

void AutocompleteControllerAndroid::ReleaseJavaObject(JNIEnv* env) {
  weak_java_autocomplete_controller_android_.reset();
}

void AutocompleteControllerAndroid::Shutdown() {
  autocomplete_controller_.reset();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge =
      weak_java_autocomplete_controller_android_.get(env);
  if (java_bridge.obj())
    Java_AutocompleteController_notifyNativeDestroyed(env, java_bridge);

  weak_java_autocomplete_controller_android_.reset();
}

// static
AutocompleteControllerAndroid*
AutocompleteControllerAndroid::Factory::GetForProfile(
    Profile* profile, JNIEnv* env, jobject obj) {
  AutocompleteControllerAndroid* bridge =
      static_cast<AutocompleteControllerAndroid*>(
          GetInstance()->GetServiceForBrowserContext(profile, true));
  bridge->InitJNI(env, obj);
  return bridge;
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
  DependsOn(ShortcutsBackendFactory::GetInstance());
}

AutocompleteControllerAndroid::Factory::~Factory() = default;

KeyedService* AutocompleteControllerAndroid::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutocompleteControllerAndroid(static_cast<Profile*>(profile));
}

AutocompleteControllerAndroid::~AutocompleteControllerAndroid() = default;

void AutocompleteControllerAndroid::InitJNI(JNIEnv* env, jobject obj) {
  weak_java_autocomplete_controller_android_ =
      JavaObjectWeakGlobalRef(env, obj);
}

void AutocompleteControllerAndroid::OnResultChanged(
    AutocompleteController* controller,
    bool default_match_changed) {
  DCHECK(controller == autocomplete_controller_.get());
  if (autocomplete_controller_ && !inside_synchronous_start_)
    NotifySuggestionsReceived(autocomplete_controller_->result());
}

void AutocompleteControllerAndroid::NotifySuggestionsReceived(
    const AutocompleteResult& autocomplete_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge =
      weak_java_autocomplete_controller_android_.get(env);
  if (!java_bridge.obj())
    return;

  autocomplete_controller_->InlineTailPrefixes();

  // Get the inline-autocomplete text.
  std::u16string inline_autocompletion;
  if (auto* default_match = autocomplete_result.default_match())
    inline_autocompletion = default_match->inline_autocompletion;
  ScopedJavaLocalRef<jstring> inline_text =
      ConvertUTF16ToJavaString(env, inline_autocompletion);

  jlong j_autocomplete_result_raw_ptr =
      reinterpret_cast<intptr_t>(&autocomplete_result);
  Java_AutocompleteController_onSuggestionsReceived(
      env, java_bridge, autocomplete_result.GetOrCreateJavaObject(env),
      inline_text, j_autocomplete_result_raw_ptr);
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

bool AutocompleteControllerAndroid::IsValidMatch(JNIEnv* env,
                                                 jint selected_index,
                                                 jint hash_code) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (base::checked_cast<size_t>(selected_index) >= result.size()) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchValidationResult::BAD_RESULT_SIZE,
                              MatchValidationResult::COUNT);
    DCHECK(!base::FeatureList::IsEnabled(omnibox::kNativeVoiceSuggestProvider))
        << "No match at position " << selected_index
        << ": Autocomplete result size mismatch.";

    return false;
  }

  // TODO(mariakhomenko): After we get results from the histogram, if invalid
  // match count is very low, we can consider skipping the expensive
  // verification step and removing this code.
  bool equal = Java_AutocompleteController_isEquivalentOmniboxSuggestion(
      env, result.match_at(selected_index).GetOrCreateJavaObject(env),
      hash_code);
  UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                            equal ? MatchValidationResult::VALID_MATCH
                                  : MatchValidationResult::WRONG_MATCH,
                            MatchValidationResult::COUNT);

  if (!equal &&
      base::FeatureList::IsEnabled(omnibox::kNativeVoiceSuggestProvider)) {
#ifndef NDEBUG
    int index = 0;
    for (const auto& match : result) {
      DLOG(WARNING) << "Native suggestion " << index << ": "
                    << match.fill_into_edit << " (" << match.provider->GetName()
                    << ", " << match.type << ")";
      index++;
    }
#endif
    DCHECK(false)
        << "AutocompleteMatch mismatch with native-sourced suggestions.";
  }

  return equal;
}

static jlong JNI_AutocompleteController_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return 0;

  AutocompleteControllerAndroid* native_bridge =
      AutocompleteControllerAndroid::Factory::GetForProfile(profile, env, obj);
  return reinterpret_cast<intptr_t>(native_bridge);
}

static ScopedJavaLocalRef<jstring>
JNI_AutocompleteController_QualifyPartialURLQuery(
    JNIEnv* env,
    const JavaParamRef<jstring>& jquery) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return ScopedJavaLocalRef<jstring>();
  AutocompleteMatch match;
  std::u16string query_string(ConvertJavaStringToUTF16(env, jquery));
  AutocompleteClassifierFactory::GetForProfile(profile)->Classify(
      query_string,
      false,
      false,
      OmniboxEventProto::INVALID_SPEC,
      &match,
      nullptr);
  if (!match.destination_url.is_valid())
    return ScopedJavaLocalRef<jstring>();

  // Only return a URL if the match is a URL type.
  if (match.type != AutocompleteMatchType::URL_WHAT_YOU_TYPED &&
      match.type != AutocompleteMatchType::HISTORY_URL &&
      match.type != AutocompleteMatchType::NAVSUGGEST)
    return ScopedJavaLocalRef<jstring>();

  // As we are returning to Java, it is fine to call Release().
  return ConvertUTF8ToJavaString(env, match.destination_url.spec());
}

static void JNI_AutocompleteController_PrefetchZeroSuggestResults(JNIEnv* env) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  // ZeroSuggestPrefetcher deletes itself after it's done prefetching.
  new ZeroSuggestPrefetcher(profile);
}
