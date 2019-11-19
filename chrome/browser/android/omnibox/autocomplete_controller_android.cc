// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include <stddef.h>

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
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_event_global_tracker.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_log.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/window_open_disposition.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
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
      match_type != AutocompleteMatchType::CLIPBOARD_TEXT) {
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
  }
}

/**
 * A prefetcher class responsible for triggering zero suggest prefetch.
 * The prefetch occurs as a side-effect of calling OnOmniboxFocused() on
 * the AutocompleteController object.
 */
class ZeroSuggestPrefetcher : public AutocompleteControllerDelegate {
 public:
  explicit ZeroSuggestPrefetcher(Profile* profile);

 private:
  ~ZeroSuggestPrefetcher() override;
  void SelfDestruct();

  // AutocompleteControllerDelegate:
  void OnResultChanged(bool default_match_changed) override;

  std::unique_ptr<AutocompleteController> controller_;
  base::OneShotTimer expire_timer_;
};

ZeroSuggestPrefetcher::ZeroSuggestPrefetcher(Profile* profile)
    : controller_(new AutocompleteController(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          this,
          AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
  // Creating an arbitrary fake_request_source to avoid passing in an invalid
  // AutocompleteInput object. This source is ignored entirely when
  // kZeroSuggestionsOnNTP feature flag is enabled.
  base::string16 fake_request_source = base::ASCIIToUTF16("chrome://newtab");
  base::string16 fake_omnibox_content;
  auto context =
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;

  if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestionsOnNTP)) {
    fake_request_source = base::ASCIIToUTF16("http://www.foobarbazblah.com");
    fake_omnibox_content = fake_request_source;
    context = metrics::OmniboxEventProto::OTHER;
  }

  AutocompleteInput input(fake_omnibox_content, context,
                          ChromeAutocompleteSchemeClassifier(profile));
  input.set_current_url(GURL(fake_request_source));
  input.set_from_omnibox_focus(true);
  controller_->Start(input);
  // Delete ourselves after 10s. This is enough time to cache results or
  // give up if the results haven't been received.
  expire_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(10000),
                      this, &ZeroSuggestPrefetcher::SelfDestruct);
}

ZeroSuggestPrefetcher::~ZeroSuggestPrefetcher() {
}

void ZeroSuggestPrefetcher::SelfDestruct() {
  delete this;
}

void ZeroSuggestPrefetcher::OnResultChanged(bool default_match_changed) {
  // Nothing to do here, the results have been cached.
  // We don't want to trigger deletion here because this is being called by the
  // AutocompleteController object.
}

}  // namespace

AutocompleteControllerAndroid::AutocompleteControllerAndroid(Profile* profile)
    : autocomplete_controller_(new AutocompleteController(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          this,
          AutocompleteClassifier::DefaultOmniboxProviders())),
      inside_synchronous_start_(false),
      profile_(profile) {}

void AutocompleteControllerAndroid::Start(JNIEnv* env,
                                          const JavaRef<jobject>& obj,
                                          const JavaRef<jstring>& j_text,
                                          jint j_cursor_pos,
                                          const JavaRef<jstring>& j_desired_tld,
                                          const JavaRef<jstring>& j_current_url,
                                          jint j_page_classification,
                                          bool prevent_inline_autocomplete,
                                          bool prefer_keyword,
                                          bool allow_exact_keyword_match,
                                          bool want_asynchronous_matches) {
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
  if (omnibox_text.empty() &&
      !current_url.SchemeIs(content::kChromeUIScheme) &&
      !current_url.SchemeIs(chrome::kChromeUINativeScheme))
    omnibox_text = url;

  input_ = AutocompleteInput(
      omnibox_text,
      OmniboxEventProto::PageClassification(j_page_classification),
      ChromeAutocompleteSchemeClassifier(profile_));
  input_.set_current_url(current_url);
  input_.set_current_title(current_title);
  input_.set_from_omnibox_focus(true);
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

  UMA_HISTOGRAM_BOOLEAN(
      "Omnibox.SuggestionUsed.RichEntity",
      match.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY);

  RecordClipboardMetrics(match.type);

  AutocompleteMatch::LogSearchEngineUsed(
      match, TemplateURLServiceFactory::GetForProfile(profile_));

  OmniboxLog log(
      // For zero suggest, record an empty input string instead of the
      // current URL.
      input_.from_omnibox_focus() ? base::string16() : input_.text(),
      false,                /* don't know */
      input_.type(), false, /* not keyword mode */
      OmniboxEventProto::INVALID, true, selected_index,
      WindowOpenDisposition::CURRENT_TAB, false,
      SessionTabHelper::IdForTab(web_contents),
      OmniboxEventProto::PageClassification(j_page_classification),
      base::TimeDelta::FromMilliseconds(elapsed_time_since_first_modified),
      completed_length,
      now - autocomplete_controller_->last_time_default_match_changed(),
      autocomplete_controller_->result());
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

ScopedJavaLocalRef<jstring> AutocompleteControllerAndroid::
    UpdateMatchDestinationURLWithQueryFormulationTime(
        JNIEnv* env,
        const JavaParamRef<jobject>& obj,
        jint selected_index,
        jint hash_code,
        jlong elapsed_time_since_input_change) {
  if (!IsValidMatch(env, selected_index, hash_code))
    return ScopedJavaLocalRef<jstring>();

  AutocompleteMatch match(
      autocomplete_controller_->result().match_at(selected_index));
  autocomplete_controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      base::TimeDelta::FromMilliseconds(elapsed_time_since_input_change),
      &match);
  return ConvertUTF8ToJavaString(env, match.destination_url.spec());
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

AutocompleteControllerAndroid::Factory::~Factory() {
}

KeyedService* AutocompleteControllerAndroid::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutocompleteControllerAndroid(static_cast<Profile*>(profile));
}

AutocompleteControllerAndroid::~AutocompleteControllerAndroid() {
}

void AutocompleteControllerAndroid::InitJNI(JNIEnv* env, jobject obj) {
  weak_java_autocomplete_controller_android_ =
      JavaObjectWeakGlobalRef(env, obj);
}

void AutocompleteControllerAndroid::OnResultChanged(
    bool default_match_changed) {
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

  ScopedJavaLocalRef<jobject> suggestion_list_obj =
      Java_AutocompleteController_createOmniboxSuggestionList(
          env, autocomplete_result.size());
  for (size_t i = 0; i < autocomplete_result.size(); ++i) {
    ScopedJavaLocalRef<jobject> j_omnibox_suggestion =
        BuildOmniboxSuggestion(env, autocomplete_result.match_at(i));
    Java_AutocompleteController_addOmniboxSuggestionToList(
        env, suggestion_list_obj, j_omnibox_suggestion);
  }

  // Get the inline-autocomplete text.
  const AutocompleteResult::const_iterator default_match(
      autocomplete_result.default_match());
  base::string16 inline_autocomplete_text;
  if (default_match != autocomplete_result.end()) {
    inline_autocomplete_text = default_match->inline_autocompletion;
  }
  ScopedJavaLocalRef<jstring> inline_text =
      ConvertUTF16ToJavaString(env, inline_autocomplete_text);
  jlong j_autocomplete_result =
      reinterpret_cast<intptr_t>(&(autocomplete_result));
  Java_AutocompleteController_onSuggestionsReceived(
      env, java_bridge, suggestion_list_obj, inline_text,
      j_autocomplete_result);
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
          0 , static_cast<int>(classification.offset) - match_offset_delta);
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
  ScopedJavaLocalRef<jstring> destination_url =
      ConvertUTF8ToJavaString(env, match.destination_url.spec());
  ScopedJavaLocalRef<jstring> image_url;
  ScopedJavaLocalRef<jstring> image_dominant_color;

  if (!match.image_url.empty()) {
    image_url = ConvertUTF8ToJavaString(env, match.image_url);
  }

  if (!match.image_dominant_color.empty()) {
    image_dominant_color =
        ConvertUTF8ToJavaString(env, match.image_dominant_color);
  }

  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  return Java_AutocompleteController_buildOmniboxSuggestion(
      env, match.type, AutocompleteMatch::IsSearchType(match.type),
      match.relevance, match.transition, jcontents,
      ToJavaIntArray(env, contents_class_offsets),
      ToJavaIntArray(env, contents_class_styles), description,
      ToJavaIntArray(env, description_class_offsets),
      ToJavaIntArray(env, description_class_styles), janswer, fill_into_edit,
      destination_url, image_url, image_dominant_color,
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url),
      match.SupportsDeletion());
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
        false, false, false, focused_from_fakebox);
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

  // ZeroSuggestPrefetcher uses a fake AutocompleteInput classified as OTHER.
  // See its constructor.
  if (!base::FeatureList::IsEnabled(omnibox::kZeroSuggestionsOnNTP) &&
      !base::Contains(
          OmniboxFieldTrial::GetZeroSuggestVariants(OmniboxEventProto::OTHER),
          ZeroSuggestProvider::kRemoteNoUrlVariant)) {
    return;
  }

  // ZeroSuggestPrefetcher deletes itself after it's done prefetching.
  new ZeroSuggestPrefetcher(profile);
}
