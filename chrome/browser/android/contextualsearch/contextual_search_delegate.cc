// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/contextualsearch/contextual_search_field_trial.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/proto/client_discourse_context.pb.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "components/contextual_search/core/browser/public.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using language::LanguageModel;
using unified_consent::UrlKeyedDataCollectionConsentHelper;

namespace {

const char kContextualSearchResponseDisplayTextParam[] = "display_text";
const char kContextualSearchResponseSelectedTextParam[] = "selected_text";
const char kContextualSearchResponseSearchTermParam[] = "search_term";
const char kContextualSearchResponseLanguageParam[] = "lang";
const char kContextualSearchResponseMidParam[] = "mid";
const char kContextualSearchResponseResolvedTermParam[] = "resolved_term";
const char kContextualSearchPreventPreload[] = "prevent_preload";
const char kContextualSearchMentions[] = "mentions";
const char kContextualSearchCaption[] = "caption";
const char kContextualSearchThumbnail[] = "thumbnail";
const char kContextualSearchAction[] = "action";
const char kContextualSearchCategory[] = "category";
const char kContextualSearchCardTag[] = "card_tag";
const char kContextualSearchSearchUrlFull[] = "search_url_full";
const char kContextualSearchSearchUrlPreload[] = "search_url_preload";

const char kActionCategoryAddress[] = "ADDRESS";
const char kActionCategoryEmail[] = "EMAIL";
const char kActionCategoryEvent[] = "EVENT";
const char kActionCategoryPhone[] = "PHONE";
const char kActionCategoryWebsite[] = "WEBSITE";

const char kContextualSearchServerEndpoint[] = "_/contextualsearch?";
const int kContextualSearchRequestVersion = 2;
const int kContextualSearchMaxSelection = 100;
const char kXssiEscape[] = ")]}'\n";
const char kDiscourseContextHeaderPrefix[] = "X-Additional-Discourse-Context: ";
const char kDoPreventPreloadValue[] = "1";

const int kResponseCodeUninitialized = -1;

}  // namespace

// Handles tasks for the ContextualSearchManager in a separable, testable way.
ContextualSearchDelegate::ContextualSearchDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service,
    const ContextualSearchDelegate::SearchTermResolutionCallback&
        search_term_callback,
    const ContextualSearchDelegate::SurroundingTextCallback&
        surrounding_text_callback)
    : url_loader_factory_(std::move(url_loader_factory)),
      template_url_service_(template_url_service),
      search_term_callback_(search_term_callback),
      surrounding_text_callback_(surrounding_text_callback) {
  field_trial_.reset(new ContextualSearchFieldTrial());
}

ContextualSearchDelegate::~ContextualSearchDelegate() {
}

void ContextualSearchDelegate::GatherAndSaveSurroundingText(
    base::WeakPtr<ContextualSearchContext> contextual_search_context,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  blink::mojom::LocalFrame::GetTextSurroundingSelectionCallback callback =
      base::BindOnce(
          &ContextualSearchDelegate::OnTextSurroundingSelectionAvailable,
          AsWeakPtr());
  context_ = contextual_search_context;
  if (context_ == nullptr)
    return;

  context_->SetBasePageEncoding(web_contents->GetEncoding());
  int surroundingTextSize = context_->CanResolve()
                                ? field_trial_->GetResolveSurroundingSize()
                                : field_trial_->GetSampleSurroundingSize();
  RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  if (focused_frame) {
    focused_frame->RequestTextSurroundingSelection(std::move(callback),
                                                   surroundingTextSize);
  } else {
    std::move(callback).Run(base::string16(), 0, 0);
  }
}

void ContextualSearchDelegate::SetActiveContext(
    base::WeakPtr<ContextualSearchContext> contextual_search_context) {
  context_ = contextual_search_context;
}

void ContextualSearchDelegate::StartSearchTermResolutionRequest(
    base::WeakPtr<ContextualSearchContext> contextual_search_context,
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (context_ == nullptr)
    return;

  DCHECK(context_.get() == contextual_search_context.get());
  DCHECK(context_->CanResolve());

  // Immediately cancel any request that's in flight, since we're building a new
  // context (and the response disposes of any existing context).
  url_loader_.reset();

  // Decide if the URL should be sent with the context.
  GURL page_url(web_contents->GetURL());
  if (context_->CanSendBasePageUrl() &&
      CanSendPageURL(page_url, ProfileManager::GetActiveUserProfile(),
                     template_url_service_)) {
    context_->SetBasePageUrl(page_url);
  }
  ResolveSearchTermFromContext();
}

void ContextualSearchDelegate::ResolveSearchTermFromContext() {
  DCHECK(context_ != nullptr);
  GURL request_url(BuildRequestUrl(context_.get()));
  DCHECK(request_url.is_valid());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = request_url;

  // Populates the discourse context and adds it to the HTTP header of the
  // search term resolution request.
  resource_request->headers.AddHeadersFromString(
      GetDiscourseContext(*context_));

  // Disable cookies for this request.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  // Add Chrome experiment state to the request headers.
  // Reset will delete any previous loader, and we won't get any callback.
  url_loader_ =
      variations::CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
          std::move(resource_request),
          variations::InIncognito::kNo,  // Impossible to be incognito at this
                                         // point.
          NO_TRAFFIC_ANNOTATION_YET);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ContextualSearchDelegate::OnUrlLoadComplete,
                     base::Unretained(this)));
}

void ContextualSearchDelegate::OnUrlLoadComplete(
    std::unique_ptr<std::string> response_body) {
  if (!context_)
    return;

  int response_code = kResponseCodeUninitialized;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  std::unique_ptr<ResolvedSearchTerm> resolved_search_term(
      new ResolvedSearchTerm(response_code));
  if (response_body && response_code == net::HTTP_OK) {
    resolved_search_term =
        GetResolvedSearchTermFromJson(response_code, *response_body);
  }
  search_term_callback_.Run(*resolved_search_term);
}

std::unique_ptr<ResolvedSearchTerm>
ContextualSearchDelegate::GetResolvedSearchTermFromJson(
    int response_code,
    const std::string& json_string) {
  DCHECK(context_ != nullptr);
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string mid;
  std::string prevent_preload;
  int mention_start = 0;
  int mention_end = 0;
  int start_adjust = 0;
  int end_adjust = 0;
  std::string context_language;
  std::string thumbnail_url = "";
  std::string caption = "";
  std::string quick_action_uri = "";
  QuickActionCategory quick_action_category = QUICK_ACTION_CATEGORY_NONE;
  int64_t logged_event_id = 0;
  std::string search_url_full = "";
  std::string search_url_preload = "";
  int coca_card_tag = 0;

  DecodeSearchTermFromJsonResponse(
      json_string, &search_term, &display_text, &alternate_term, &mid,
      &prevent_preload, &mention_start, &mention_end, &context_language,
      &thumbnail_url, &caption, &quick_action_uri, &quick_action_category,
      &logged_event_id, &search_url_full, &search_url_preload, &coca_card_tag);
  if (mention_start != 0 || mention_end != 0) {
    // Sanity check that our selection is non-zero and it is less than
    // 100 characters as that would make contextual search bar hide.
    // We also check that there is at least one character overlap between
    // the new and old selection.
    if (mention_start >= mention_end ||
        (mention_end - mention_start) > kContextualSearchMaxSelection ||
        mention_end <= context_->GetStartOffset() ||
        mention_start >= context_->GetEndOffset()) {
      start_adjust = 0;
      end_adjust = 0;
    } else {
      start_adjust = mention_start - context_->GetStartOffset();
      end_adjust = mention_end - context_->GetEndOffset();
    }
  }
  bool is_invalid = response_code == kResponseCodeUninitialized;
  return std::unique_ptr<ResolvedSearchTerm>(new ResolvedSearchTerm(
      is_invalid, response_code, search_term, display_text, alternate_term, mid,
      prevent_preload == kDoPreventPreloadValue, start_adjust, end_adjust,
      context_language, thumbnail_url, caption, quick_action_uri,
      quick_action_category, logged_event_id, search_url_full,
      search_url_preload, coca_card_tag));
}

std::string ContextualSearchDelegate::BuildRequestUrl(
    ContextualSearchContext* context) {
  if (!template_url_service_ ||
      !template_url_service_->GetDefaultSearchProvider()) {
    return std::string();
  }

  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::string16());

  // Set the Coca-integration version.
  // This is based on our current active feature, or an override param from a
  // field trial, possibly augmented by using simplified server logic.
  int contextual_cards_version =
      contextual_search::kContextualCardsUrlActionsIntegration;
  if (base::FeatureList::IsEnabled(
                 chrome::android::kContextualSearchDefinitions)) {
    contextual_cards_version =
        contextual_search::kContextualCardsDefinitionsIntegration;
  }
  // Let the field-trial override.
  if (field_trial_->GetContextualCardsVersion() != 0) {
    contextual_cards_version = field_trial_->GetContextualCardsVersion();
  }
  // Add the simplified-server mixin, if enabled.
  if (base::FeatureList::IsEnabled(
          chrome::android::kContextualSearchSimplifiedServer) &&
      contextual_cards_version <
          contextual_search::kContextualCardsSimplifiedServerMixin) {
    contextual_cards_version =
        contextual_cards_version +
        contextual_search::kContextualCardsSimplifiedServerMixin;
  }

  TemplateURLRef::SearchTermsArgs::ContextualSearchParams params(
      kContextualSearchRequestVersion, contextual_cards_version,
      context->GetHomeCountry(), context->GetPreviousEventId(),
      context->GetPreviousEventResults());

  search_terms_args.contextual_search_params = params;

  std::string request(
      template_url->contextual_search_url_ref().ReplaceSearchTerms(
          search_terms_args,
          template_url_service_->search_terms_data(),
          NULL));

  // The switch/param should be the URL up to and including the endpoint.
  std::string replacement_url = field_trial_->GetResolverURLPrefix();

  // If a replacement URL was specified above, do the substitution.
  if (!replacement_url.empty()) {
    size_t pos = request.find(kContextualSearchServerEndpoint);
    if (pos != std::string::npos) {
      request.replace(0, pos + strlen(kContextualSearchServerEndpoint),
                      replacement_url);
    }
  }
  return request;
}

void ContextualSearchDelegate::OnTextSurroundingSelectionAvailable(
    const base::string16& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  if (context_ == nullptr)
    return;

  // Sometimes the surroundings are 0, 0, '', so run the callback with empty
  // data in that case. See https://crbug.com/393100.
  if (start_offset == 0 && end_offset == 0 && surrounding_text.length() == 0) {
    surrounding_text_callback_.Run(std::string(), base::string16(), 0, 0);
    return;
  }

  // Pin the start and end offsets to ensure they point within the string.
  uint32_t surrounding_length = surrounding_text.length();
  start_offset = std::min(surrounding_length, start_offset);
  end_offset = std::min(surrounding_length, end_offset);

  context_->SetSelectionSurroundings(start_offset, end_offset,
                                     surrounding_text);

  // Call the Java surrounding callback with a shortened copy of the
  // surroundings to use as a sample of the surrounding text.
  int sample_surrounding_size = field_trial_->GetSampleSurroundingSize();
  DCHECK(sample_surrounding_size >= 0);
  DCHECK(start_offset <= end_offset);
  size_t selection_start = start_offset;
  size_t selection_end = end_offset;
  int sample_padding_each_side = sample_surrounding_size / 2;
  base::string16 sample_surrounding_text =
      SampleSurroundingText(surrounding_text, sample_padding_each_side,
                            &selection_start, &selection_end);
  DCHECK(selection_start <= selection_end);
  surrounding_text_callback_.Run(context_->GetBasePageEncoding(),
                                 sample_surrounding_text, selection_start,
                                 selection_end);
}

std::string ContextualSearchDelegate::GetDiscourseContext(
    const ContextualSearchContext& context) {
  discourse_context::ClientDiscourseContext proto;
  discourse_context::Display* display = proto.add_display();
  display->set_uri(context.GetBasePageUrl().spec());

  discourse_context::Media* media = display->mutable_media();
  media->set_mime_type(context.GetBasePageEncoding());

  discourse_context::Selection* selection = display->mutable_selection();
  selection->set_content(base::UTF16ToUTF8(context.GetSurroundingText()));
  selection->set_start(context.GetStartOffset());
  selection->set_end(context.GetEndOffset());
  selection->set_is_uri_encoded(false);

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string encoded_context;
  base::Base64Encode(serialized, &encoded_context);
  // The server memoizer expects a web-safe encoding.
  std::replace(encoded_context.begin(), encoded_context.end(), '+', '-');
  std::replace(encoded_context.begin(), encoded_context.end(), '/', '_');
  return kDiscourseContextHeaderPrefix + encoded_context;
}

bool ContextualSearchDelegate::CanSendPageURL(
    const GURL& current_page_url,
    Profile* profile,
    TemplateURLService* template_url_service) {
  // Check whether there is a Finch parameter preventing us from sending the
  // page URL.
  if (field_trial_->IsSendBasePageURLDisabled())
    return false;

  // Ensure that the default search provider is Google.
  const TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  bool is_default_search_provider_google =
      default_search_provider &&
      default_search_provider->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data());
  if (!is_default_search_provider_google)
    return false;

  // Only allow HTTP URLs or HTTPS URLs.
  if (current_page_url.scheme() != url::kHttpScheme &&
      (current_page_url.scheme() != url::kHttpsScheme))
    return false;

  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  if (!sync_service)
    return false;

  // Check whether the user has enabled anonymous URL-keyed data collection
  // from the unified consent service.
  std::unique_ptr<UrlKeyedDataCollectionConsentHelper>
      anonymized_unified_consent_url_helper =
          UrlKeyedDataCollectionConsentHelper::
              NewAnonymizedDataCollectionConsentHelper(
                  ProfileManager::GetActiveUserProfile()->GetPrefs(),
                  sync_service);
  // If they have, then allow sending of the URL.
  return anonymized_unified_consent_url_helper->IsEnabled();
}

// Gets the target language from the translate service using the user's profile.
std::string ContextualSearchDelegate::GetTargetLanguage() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetPrimaryModel();
  DCHECK(language_model);
  PrefService* pref_service = profile->GetPrefs();
  std::string result =
      TranslateService::GetTargetLanguage(pref_service, language_model);
  DCHECK(!result.empty());
  return result;
}

// Returns the accept languages preference string.
std::string ContextualSearchDelegate::GetAcceptLanguages() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  return pref_service->GetString(language::prefs::kAcceptLanguages);
}

// Decodes the given response from the search term resolution request and sets
// the value of the given parameters.
void ContextualSearchDelegate::DecodeSearchTermFromJsonResponse(
    const std::string& response,
    std::string* search_term,
    std::string* display_text,
    std::string* alternate_term,
    std::string* mid,
    std::string* prevent_preload,
    int* mention_start,
    int* mention_end,
    std::string* lang,
    std::string* thumbnail_url,
    std::string* caption,
    std::string* quick_action_uri,
    QuickActionCategory* quick_action_category,
    int64_t* logged_event_id,
    std::string* search_url_full,
    std::string* search_url_preload,
    int* coca_card_tag) {
  bool contains_xssi_escape =
      base::StartsWith(response, kXssiEscape, base::CompareCase::SENSITIVE);
  const std::string& proper_json =
      contains_xssi_escape ? response.substr(sizeof(kXssiEscape) - 1)
                           : response;
  JSONStringValueDeserializer deserializer(proper_json);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);
  const std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(root));
  if (!dict)
    return;

  dict->GetString(kContextualSearchPreventPreload, prevent_preload);
  dict->GetString(kContextualSearchResponseSearchTermParam, search_term);
  dict->GetString(kContextualSearchResponseLanguageParam, lang);

  // For the display_text, if not present fall back to the "search_term".
  if (!dict->GetString(kContextualSearchResponseDisplayTextParam,
                       display_text)) {
    *display_text = *search_term;
  }
  dict->GetString(kContextualSearchResponseMidParam, mid);

  // Extract mentions for selection expansion.
  if (!field_trial_->IsDecodeMentionsDisabled()) {
    base::ListValue* mentions_list = nullptr;
    dict->GetList(kContextualSearchMentions, &mentions_list);
    if (mentions_list && mentions_list->GetSize() >= 2)
      ExtractMentionsStartEnd(*mentions_list, mention_start, mention_end);
  }

  // If either the selected text or the resolved term is not the search term,
  // use it as the alternate term.
  std::string selected_text;
  dict->GetString(kContextualSearchResponseSelectedTextParam, &selected_text);
  if (selected_text != *search_term) {
    *alternate_term = selected_text;
  } else {
    std::string resolved_term;
    dict->GetString(kContextualSearchResponseResolvedTermParam, &resolved_term);
    if (resolved_term != *search_term) {
      *alternate_term = resolved_term;
    }
  }

  // Contextual Cards V1+ Integration.
  // Get the basic Bar data for Contextual Cards integration directly
  // from the root.
  dict->GetString(kContextualSearchCaption, caption);
  dict->GetString(kContextualSearchThumbnail, thumbnail_url);

  // Contextual Cards V2+ Integration.
  // Get the Single Action data.
  dict->GetString(kContextualSearchAction, quick_action_uri);
  std::string quick_action_category_string;
  dict->GetString(kContextualSearchCategory, &quick_action_category_string);
  if (!quick_action_category_string.empty()) {
    if (quick_action_category_string == kActionCategoryAddress) {
      *quick_action_category = QUICK_ACTION_CATEGORY_ADDRESS;
    } else if (quick_action_category_string == kActionCategoryEmail) {
      *quick_action_category = QUICK_ACTION_CATEGORY_EMAIL;
    } else if (quick_action_category_string == kActionCategoryEvent) {
      *quick_action_category = QUICK_ACTION_CATEGORY_EVENT;
    } else if (quick_action_category_string == kActionCategoryPhone) {
      *quick_action_category = QUICK_ACTION_CATEGORY_PHONE;
    } else if (quick_action_category_string == kActionCategoryWebsite) {
      *quick_action_category = QUICK_ACTION_CATEGORY_WEBSITE;
    }
  }

  // Contextual Cards V4+ may also provide full search URLs to use in the
  // overlay.
  dict->GetString(kContextualSearchSearchUrlFull, search_url_full);
  dict->GetString(kContextualSearchSearchUrlPreload, search_url_preload);

  // Contextual Cards V5+ integration can provide the primary card tag, so
  // clients can tell what kind of card they have received.
  // TODO(donnd): make sure this works with a non-integer or missing value!
  dict->GetInteger(kContextualSearchCardTag, coca_card_tag);

  // Any Contextual Cards integration.
  // For testing purposes check if there was a diagnostic from Contextual
  // Cards and output that into the log.
  // TODO(donnd): remove after full Contextual Cards integration.
  std::string contextual_cards_diagnostic;
  dict->GetString("diagnostic", &contextual_cards_diagnostic);
  if (contextual_cards_diagnostic.empty()) {
    DVLOG(0) << "No diagnostic data in the response.";
  } else {
    DVLOG(0) << "The Contextual Cards backend response: ";
    DVLOG(0) << contextual_cards_diagnostic;
  }

  // Get the Event ID to use for sending event outcomes back to the server.
  std::string logged_event_id_string;
  dict->GetString("logged_event_id", &logged_event_id_string);
  if (!logged_event_id_string.empty()) {
    *logged_event_id = std::stoll(logged_event_id_string, nullptr);
  }
}

// Extract the Start/End of the mentions in the surrounding text
// for selection-expansion.
void ContextualSearchDelegate::ExtractMentionsStartEnd(
    const base::ListValue& mentions_list,
    int* startResult,
    int* endResult) {
  int int_value;
  if (mentions_list.GetInteger(0, &int_value))
    *startResult = std::max(0, int_value);
  if (mentions_list.GetInteger(1, &int_value))
    *endResult = std::max(0, int_value);
}

base::string16 ContextualSearchDelegate::SampleSurroundingText(
    const base::string16& surrounding_text,
    int padding_each_side,
    size_t* start,
    size_t* end) {
  base::string16 result_text = surrounding_text;
  size_t start_offset = *start;
  size_t end_offset = *end;
  size_t padding_each_side_pinned =
      padding_each_side >= 0 ? padding_each_side : 0;
  // Now trim the context so the portions before or after the selection
  // are within the given limit.
  if (start_offset > padding_each_side_pinned) {
    // Trim the start.
    int trim = start_offset - padding_each_side_pinned;
    result_text = result_text.substr(trim);
    start_offset -= trim;
    end_offset -= trim;
  }
  if (result_text.length() > end_offset + padding_each_side_pinned) {
    // Trim the end.
    result_text = result_text.substr(0, end_offset + padding_each_side_pinned);
  }
  *start = start_offset;
  *end = end_offset;
  return result_text;
}
