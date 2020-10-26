// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <stddef.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
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
  AutocompleteInput input(base::string16(), metrics::OmniboxEventProto::NTP,
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
  base::string16 text = ConvertJavaStringToUTF16(env, j_text);
  size_t cursor_pos = j_cursor_pos == -1 ? base::string16::npos : j_cursor_pos;
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
  return GetTopSynchronousResult(env, obj, j_text, true, focused_from_fakebox);
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

  base::string16 url = ConvertJavaStringToUTF16(env, j_current_url);
  base::string16 current_title = ConvertJavaStringToUTF16(env, j_current_title);
  const GURL current_url = GURL(url);
  base::string16 omnibox_text = ConvertJavaStringToUTF16(env, j_omnibox_text);

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

  base::string16 url = ConvertJavaStringToUTF16(env, j_current_url);
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

  AutocompleteMatch::LogSearchEngineUsed(
      match, TemplateURLServiceFactory::GetForProfile(profile_));

  OmniboxLog log(
      // For zero suggest, record an empty input string instead of the
      // current URL.
      input_.focus_type() != OmniboxFocusType::DEFAULT ? base::string16()
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
  autocomplete_controller_->AddProvidersInfo(&log.providers_info);

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
    base::string16 query =
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

void AutocompleteControllerAndroid::GroupSuggestionsBySearchVsURL(
    JNIEnv* /* env */,
    int first_index,
    int last_index) {
  autocomplete_controller_->result().GroupSuggestionsBySearchVsURL(first_index,
                                                                   last_index);
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

  ScopedJavaLocalRef<jobject> j_autocomplete_result =
      Java_AutocompleteController_createAutocompleteResult(
          env, autocomplete_result.size(),
          autocomplete_result.headers_map().size());

  for (size_t i = 0; i < autocomplete_result.size(); ++i) {
    ScopedJavaLocalRef<jobject> j_omnibox_suggestion =
        BuildOmniboxSuggestion(env, autocomplete_result.match_at(i));
    Java_AutocompleteController_addOmniboxSuggestionToResult(
        env, j_autocomplete_result, j_omnibox_suggestion);
  }

  PopulateOmniboxGroupsDetails(env, j_autocomplete_result,
                               autocomplete_result.headers_map(),
                               autocomplete_result.hidden_group_ids());

  // Get the inline-autocomplete text.
  base::string16 inline_autocompletion;
  if (auto* default_match = autocomplete_result.default_match())
    inline_autocompletion = default_match->inline_autocompletion;
  ScopedJavaLocalRef<jstring> inline_text =
      ConvertUTF16ToJavaString(env, inline_autocompletion);
  jlong j_autocomplete_result_raw_ptr =
      reinterpret_cast<intptr_t>(&(autocomplete_result));
  Java_AutocompleteController_onSuggestionsReceived(
      env, java_bridge, j_autocomplete_result, inline_text,
      j_autocomplete_result_raw_ptr);
}

namespace {

// Updates the formatting of Android omnibox suggestions where we intentionally
// deviate from the desktop logic.
//
// For URL suggestions, the leading https:// and www. will be omitted if the
// user query did not explicitly contain them.  The http:// portion is already
// omitted for all ports.
//
// If the match is not for a URL, it will leave |out_content| and
// |out_classifications| untouched.
void FormatMatchContentsForDisplay(
    const AutocompleteMatch& match,
    base::string16* out_content,
    ACMatchClassifications* out_classifications) {
  if (AutocompleteMatch::IsSearchType(match.type))
    return;

  int match_offset = -1;
  for (auto contents_class : match.contents_class) {
    if (contents_class.style & ACMatchClassification::MATCH) {
      match_offset = contents_class.offset;
      break;
    }
  }
  int original_match_offset(match_offset);
  const base::string16 https(base::ASCIIToUTF16("https://"));
  if (base::StartsWith(*out_content, https, base::CompareCase::SENSITIVE)) {
    if (match_offset >= static_cast<int>(https.length())) {
      *out_content = out_content->substr(https.length());
      match_offset -= https.length();
    }
  }
  const base::string16 www(base::ASCIIToUTF16("www."));
  if (base::StartsWith(*out_content, www, base::CompareCase::SENSITIVE)) {
    if (match_offset >= static_cast<int>(www.length())) {
      *out_content = out_content->substr(www.length());
      match_offset -= www.length();
    }
  }
  int match_offset_delta = original_match_offset - match_offset;
  if (match_offset_delta > 0) {
    out_classifications->clear();
    for (size_t i = match.contents_class.size(); i > 0; --i) {
      ACMatchClassification classification(match.contents_class[i - 1]);
      int updated_offset = std::max(
          0, static_cast<int>(classification.offset) - match_offset_delta);
      out_classifications->insert(
          out_classifications->begin(),
          ACMatchClassification(updated_offset, classification.style));
      if (updated_offset == 0)
        break;
    }
  }
}

}  // namespace

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::BuildOmniboxSuggestion(
    JNIEnv* env,
    const AutocompleteMatch& match) {
  base::string16 contents(match.contents);
  ACMatchClassifications contents_classifications(match.contents_class);
  FormatMatchContentsForDisplay(match, &contents, &contents_classifications);

  ScopedJavaLocalRef<jstring> jcontents =
      ConvertUTF16ToJavaString(env, contents);
  std::vector<int> contents_class_offsets;
  std::vector<int> contents_class_styles;
  for (auto contents_class : contents_classifications) {
    contents_class_offsets.push_back(contents_class.offset);
    contents_class_styles.push_back(contents_class.style);
  }

  ScopedJavaLocalRef<jstring> description =
      ConvertUTF16ToJavaString(env, match.description);
  std::vector<int> description_class_offsets;
  std::vector<int> description_class_styles;
  for (auto description_class : match.description_class) {
    description_class_offsets.push_back(description_class.offset);
    description_class_styles.push_back(description_class.style);
  }

  ScopedJavaLocalRef<jobject> janswer;
  if (match.answer)
    janswer = match.answer->CreateJavaObject();
  ScopedJavaLocalRef<jstring> fill_into_edit =
      ConvertUTF16ToJavaString(env, match.fill_into_edit);
  ScopedJavaLocalRef<jobject> destination_url =
      url::GURLAndroid::FromNativeGURL(env, match.destination_url);
  ScopedJavaLocalRef<jobject> image_url =
      url::GURLAndroid::FromNativeGURL(env, match.image_url);
  ScopedJavaLocalRef<jstring> image_dominant_color;
  ScopedJavaLocalRef<jstring> post_content_type;
  std::string post_content;
  std::string clipboard_image_data;

  if (!match.image_dominant_color.empty()) {
    image_dominant_color =
        ConvertUTF8ToJavaString(env, match.image_dominant_color);
  }

  if (match.post_content.get()) {
    if (!match.post_content.get()->first.empty()) {
      post_content_type =
          ConvertUTF8ToJavaString(env, match.post_content.get()->first);
    }
    if (!match.post_content.get()->second.empty()) {
      post_content = match.post_content.get()->second;
    }
  }

  if (match.search_terms_args.get()) {
    clipboard_image_data = match.search_terms_args->image_thumbnail_content;
  }

  ScopedJavaLocalRef<jobject> j_query_tiles =
      query_tiles::TileConversionBridge::CreateJavaTiles(env,
                                                         match.query_tiles);
  ScopedJavaLocalRef<jobject> j_navsuggest_tiles =
      BuildNavsuggestTilesList(env, match.navsuggest_tiles);

  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const std::vector<int> subtypes(match.subtypes.begin(), match.subtypes.end());
  return Java_AutocompleteController_buildOmniboxSuggestion(
      env, match.type, ToJavaIntArray(env, subtypes),
      AutocompleteMatch::IsSearchType(match.type), match.relevance,
      match.transition, jcontents, ToJavaIntArray(env, contents_class_offsets),
      ToJavaIntArray(env, contents_class_styles), description,
      ToJavaIntArray(env, description_class_offsets),
      ToJavaIntArray(env, description_class_styles), janswer, fill_into_edit,
      destination_url, image_url, image_dominant_color,
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url),
      match.SupportsDeletion(), post_content_type,
      ToJavaByteArray(env, post_content),
      match.suggestion_group_id.value_or(
          SearchSuggestionParser::kNoSuggestionGroupId),
      j_query_tiles, ToJavaByteArray(env, clipboard_image_data),
      match.has_tab_match, j_navsuggest_tiles);
}

base::android::ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::BuildNavsuggestTilesList(
    JNIEnv* env,
    const std::vector<AutocompleteMatch::NavsuggestTile>& tiles) {
  if (tiles.empty())
    return ScopedJavaLocalRef<jobject>();
  ScopedJavaLocalRef<jobject> j_navsuggest_tiles =
      Java_AutocompleteController_buildOmniboxNavsuggestTileList(env,
                                                                 tiles.size());
  for (const auto& tile : tiles) {
    ScopedJavaLocalRef<jstring> title =
        ConvertUTF16ToJavaString(env, tile.title);
    ScopedJavaLocalRef<jobject> url =
        url::GURLAndroid::FromNativeGURL(env, tile.url);
    Java_AutocompleteController_addOmniboxNavsuggestTile(
        env, j_navsuggest_tiles, title, url);
  }
  return j_navsuggest_tiles;
}

void AutocompleteControllerAndroid::PopulateOmniboxGroupsDetails(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> j_autocomplete_result,
    const SearchSuggestionParser::HeadersMap& native_header_map,
    const std::set<int>& hidden_group_ids) {
  for (const auto& group_header : native_header_map) {
    Java_AutocompleteController_addOmniboxGroupDetailsToResult(
        env, j_autocomplete_result, group_header.first,
        ConvertUTF16ToJavaString(env, group_header.second),
        base::Contains(hidden_group_ids, group_header.first));
  }
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetTopSynchronousResult(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    const JavaRef<jstring>& j_text,
    bool prevent_inline_autocomplete,
    bool focused_from_fakebox) {
  if (!autocomplete_controller_)
    return ScopedJavaLocalRef<jobject>();

  inside_synchronous_start_ = true;
  Start(env, obj, j_text, -1, nullptr, nullptr, prevent_inline_autocomplete,
        false, false, false, focused_from_fakebox, JavaRef<jstring>(), false);
  inside_synchronous_start_ = false;
  DCHECK(autocomplete_controller_->done());
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (result.empty())
    return ScopedJavaLocalRef<jobject>();

  return BuildOmniboxSuggestion(env, *result.begin());
}

bool AutocompleteControllerAndroid::IsValidMatch(JNIEnv* env,
                                                 jint selected_index,
                                                 jint hash_code) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (base::checked_cast<size_t>(selected_index) >= result.size()) {
    UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                              MatchValidationResult::BAD_RESULT_SIZE,
                              MatchValidationResult::COUNT);
    return false;
  }

  // TODO(mariakhomenko): After we get results from the histogram, if invalid
  // match count is very low, we can consider skipping the expensive
  // verification step and removing this code.
  bool equal = Java_AutocompleteController_isEquivalentOmniboxSuggestion(
      env, BuildOmniboxSuggestion(env, result.match_at(selected_index)),
      hash_code);
  UMA_HISTOGRAM_ENUMERATION("Android.Omnibox.InvalidMatch",
                            equal ? MatchValidationResult::VALID_MATCH
                                  : MatchValidationResult::WRONG_MATCH,
                            MatchValidationResult::COUNT);
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
  base::string16 query_string(ConvertJavaStringToUTF16(env, jquery));
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
