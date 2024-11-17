// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/omnibox/omnibox_input_watcher_factory.h"
#include "chrome/browser/omnibox/omnibox_suggestions_watcher_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/extensions/api/omnibox.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "components/omnibox/browser/omnibox_input_watcher.h"
#include "components/omnibox/browser/omnibox_suggestions_watcher.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace omnibox = api::omnibox;
namespace SendSuggestions = omnibox::SendSuggestions;
namespace SetDefaultSuggestion = omnibox::SetDefaultSuggestion;

namespace {

const char kSuggestionContent[] = "content";
const char kCurrentTabDisposition[] = "currentTab";
const char kForegroundTabDisposition[] = "newForegroundTab";
const char kBackgroundTabDisposition[] = "newBackgroundTab";

// Pref key for omnibox.setDefaultSuggestion.
const char kOmniboxDefaultSuggestion[] = "omnibox_default_suggestion";

std::optional<omnibox::SuggestResult> GetOmniboxDefaultSuggestion(
    Profile* profile,
    const ExtensionId& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  if (!prefs) {
    return std::nullopt;
  }

  const base::Value::Dict* dict =
      prefs->ReadPrefAsDict(extension_id, kOmniboxDefaultSuggestion);
  if (!dict) {
    return std::nullopt;
  }
  return omnibox::SuggestResult::FromValue(*dict);
}

// Tries to set the omnibox default suggestion; returns true on success or
// false on failure.
bool SetOmniboxDefaultSuggestion(
    Profile* profile,
    const ExtensionId& extension_id,
    const omnibox::DefaultSuggestResult& suggestion) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  if (!prefs)
    return false;

  base::Value::Dict dict = suggestion.ToValue();
  // Add the content field so that the dictionary can be used to populate an
  // omnibox::SuggestResult.
  dict.Set(kSuggestionContent, base::Value(base::Value::Type::STRING));
  prefs->UpdateExtensionPref(extension_id, kOmniboxDefaultSuggestion,
                             base::Value(std::move(dict)));

  return true;
}

// Returns a string used as a template URL string of the extension.
std::string GetTemplateURLStringForExtension(const ExtensionId& extension_id) {
  // This URL is not actually used for navigation. It holds the extension's ID.
  return std::string(extensions::kExtensionScheme) + "://" +
      extension_id + "/?q={searchTerms}";
}

}  // namespace

// static
void ExtensionOmniboxEventRouter::OnInputStarted(
    Profile* profile,
    const ExtensionId& extension_id) {
  auto event = std::make_unique<Event>(events::OMNIBOX_ON_INPUT_STARTED,
                                       omnibox::OnInputStarted::kEventName,
                                       base::Value::List(), profile);
  EventRouter::Get(profile)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

// static
bool ExtensionOmniboxEventRouter::OnInputChanged(
    Profile* profile,
    const ExtensionId& extension_id,
    const std::string& input,
    int suggest_id) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!event_router->ExtensionHasEventListener(
          extension_id, omnibox::OnInputChanged::kEventName))
    return false;

  base::Value::List args;
  args.Append(input);
  args.Append(suggest_id);

  auto event = std::make_unique<Event>(events::OMNIBOX_ON_INPUT_CHANGED,
                                       omnibox::OnInputChanged::kEventName,
                                       std::move(args), profile);
  event_router->DispatchEventToExtension(extension_id, std::move(event));
  return true;
}

// static
void ExtensionOmniboxEventRouter::OnInputEntered(
    content::WebContents* web_contents,
    const ExtensionId& extension_id,
    const std::string& input,
    WindowOpenDisposition disposition) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  const Extension* extension =
      ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  CHECK(extension);
  extensions::TabHelper::FromWebContents(web_contents)->
      active_tab_permission_granter()->GrantIfRequested(extension);

  base::Value::List args;
  args.Append(input);
  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB)
    args.Append(kForegroundTabDisposition);
  else if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB)
    args.Append(kBackgroundTabDisposition);
  else
    args.Append(kCurrentTabDisposition);

  auto event = std::make_unique<Event>(events::OMNIBOX_ON_INPUT_ENTERED,
                                       omnibox::OnInputEntered::kEventName,
                                       std::move(args), profile);
  event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
  EventRouter::Get(profile)
      ->DispatchEventToExtension(extension_id, std::move(event));

  OmniboxInputWatcherFactory::GetForBrowserContext(profile)
      ->NotifyInputEntered();
}

// static
void ExtensionOmniboxEventRouter::OnInputCancelled(
    Profile* profile,
    const ExtensionId& extension_id) {
  auto event = std::make_unique<Event>(events::OMNIBOX_ON_INPUT_CANCELLED,
                                       omnibox::OnInputCancelled::kEventName,
                                       base::Value::List(), profile);
  EventRouter::Get(profile)
      ->DispatchEventToExtension(extension_id, std::move(event));
}

void ExtensionOmniboxEventRouter::OnDeleteSuggestion(
    Profile* profile,
    const ExtensionId& extension_id,
    const std::string& suggestion_text) {
  base::Value::List args;
  args.Append(suggestion_text);

  auto event = std::make_unique<Event>(events::OMNIBOX_ON_DELETE_SUGGESTION,
                                       omnibox::OnDeleteSuggestion::kEventName,
                                       std::move(args), profile);

  EventRouter::Get(profile)->DispatchEventToExtension(extension_id,
                                                      std::move(event));
}

OmniboxAPI::OmniboxAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      url_service_(TemplateURLServiceFactory::GetForProfile(profile_)) {
  extension_registry_observation_.Observe(ExtensionRegistry::Get(profile_));
  if (url_service_) {
    template_url_subscription_ =
        url_service_->RegisterOnLoadedCallback(base::BindOnce(
            &OmniboxAPI::OnTemplateURLsLoaded, base::Unretained(this)));
  }

  // Use monochrome icons for Omnibox icons.
  omnibox_icon_manager_.set_monochrome(true);
}

void OmniboxAPI::Shutdown() {
  template_url_subscription_ = {};
}

OmniboxAPI::~OmniboxAPI() {
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<OmniboxAPI>>::
    DestructorAtExit g_omnibox_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<OmniboxAPI>* OmniboxAPI::GetFactoryInstance() {
  return g_omnibox_api_factory.Pointer();
}

// static
OmniboxAPI* OmniboxAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<OmniboxAPI>::Get(context);
}

void OmniboxAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                   const Extension* extension) {
  const std::string& keyword = OmniboxInfo::GetKeyword(extension);
  if (!keyword.empty()) {
    // Load the omnibox icon so it will be ready to display in the URL bar.
    omnibox_icon_manager_.LoadIcon(profile_, extension);
    if (url_service_) {
      url_service_->Load();
      if (url_service_->loaded()) {
        url_service_->RegisterOmniboxKeyword(
            extension->id(), extension->short_name(), keyword,
            GetTemplateURLStringForExtension(extension->id()),
            ExtensionPrefs::Get(profile_)->GetLastUpdateTime(extension->id()));
      } else {
        pending_extensions_.insert(extension);
      }
    }
  }
}

void OmniboxAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                     const Extension* extension,
                                     UnloadedExtensionReason reason) {
  if (!OmniboxInfo::GetKeyword(extension).empty() && url_service_) {
    if (url_service_->loaded()) {
      url_service_->RemoveExtensionControlledTURL(
          extension->id(), TemplateURL::OMNIBOX_API_EXTENSION);
    } else {
      pending_extensions_.erase(extension);
    }
  }
}

gfx::Image OmniboxAPI::GetOmniboxIcon(const ExtensionId& extension_id) {
  return omnibox_icon_manager_.GetIcon(extension_id);
}

void OmniboxAPI::OnTemplateURLsLoaded() {
  // Register keywords for pending extensions.
  template_url_subscription_ = {};
  for (const Extension* i : pending_extensions_) {
    url_service_->RegisterOmniboxKeyword(
        i->id(), i->short_name(), OmniboxInfo::GetKeyword(i),
        GetTemplateURLStringForExtension(i->id()),
        ExtensionPrefs::Get(profile_)->GetLastUpdateTime(i->id()));
  }
  pending_extensions_.clear();
}

template <>
void BrowserContextKeyedAPIFactory<OmniboxAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

OmniboxSendSuggestionsFunction::OmniboxSendSuggestionsFunction() = default;
OmniboxSendSuggestionsFunction::~OmniboxSendSuggestionsFunction() = default;

ExtensionFunction::ResponseAction OmniboxSendSuggestionsFunction::Run() {
  params_ = SendSuggestions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  if (is_from_service_worker() && !params_->suggest_results.empty()) {
    std::vector<std::string_view> inputs;
    inputs.reserve(params_->suggest_results.size());
    for (const auto& suggestion : params_->suggest_results)
      inputs.push_back(suggestion.description);

    ParseDescriptionsAndStyles(
        inputs,
        base::BindOnce(
            &OmniboxSendSuggestionsFunction::OnParsedDescriptionsAndStyles,
            this));
    return RespondLater();
  }

  NotifySuggestionsReady();
  return RespondNow(NoArguments());
}

void OmniboxSendSuggestionsFunction::OnParsedDescriptionsAndStyles(
    DescriptionAndStylesResult result) {
  DCHECK(params_);
  // Since the XML parsing happens asynchronously, the browser context can be
  // torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  if (!result.error.empty()) {
    Respond(Error(std::move(result.error)));
    return;
  }

  if (result.descriptions_and_styles.size() !=
      params_->suggest_results.size()) {
    // This can technically happen if the extension provided input that mucked
    // with our XML parsing (see suggestion_parser_unittest.cc). This isn't a
    // security concern, but would mean that our mapping to record the other
    // fields in the suggestion are mismatched. Abort. Since there's no
    // legitimate case for this happening, just emit a generic error message.
    Respond(Error("Invalid input."));
    return;
  }

  for (size_t i = 0; i < params_->suggest_results.size(); ++i) {
    params_->suggest_results[i].description =
        base::UTF16ToUTF8(result.descriptions_and_styles[i].description);
    params_->suggest_results[i].description_styles =
        std::move(result.descriptions_and_styles[i].styles);
  }

  NotifySuggestionsReady();
  Respond(NoArguments());
}

void OmniboxSendSuggestionsFunction::NotifySuggestionsReady() {
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  OmniboxSuggestionsWatcherFactory::GetForBrowserContext(profile)
      ->NotifySuggestionsReady(&*params_);
}

ExtensionFunction::ResponseAction OmniboxSetDefaultSuggestionFunction::Run() {
  std::optional<SetDefaultSuggestion::Params> params =
      SetDefaultSuggestion::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!params->suggestion.description_styles) {
    ParseDescriptionAndStyles(
        params->suggestion.description,
        base::BindOnce(
            &OmniboxSetDefaultSuggestionFunction::OnParsedDescriptionAndStyles,
            this));
    return RespondLater();
  }

  SetDefaultSuggestion(params->suggestion);
  return RespondNow(NoArguments());
}

void OmniboxSetDefaultSuggestionFunction::OnParsedDescriptionAndStyles(
    DescriptionAndStylesResult result) {
  if (!result.error.empty()) {
    Respond(Error(std::move(result.error)));
    return;
  }

  DCHECK_EQ(1u, result.descriptions_and_styles.size());
  DescriptionAndStyles& single_result = result.descriptions_and_styles[0];

  omnibox::DefaultSuggestResult default_suggestion;
  default_suggestion.description = base::UTF16ToUTF8(single_result.description);
  default_suggestion.description_styles.emplace();
  default_suggestion.description_styles->swap(single_result.styles);
  SetDefaultSuggestion(default_suggestion);
  Respond(NoArguments());
}

void OmniboxSetDefaultSuggestionFunction::SetDefaultSuggestion(
    const omnibox::DefaultSuggestResult& suggestion) {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (SetOmniboxDefaultSuggestion(profile, extension_id(), suggestion)) {
    OmniboxSuggestionsWatcherFactory::GetForBrowserContext(
        profile->GetOriginalProfile())
        ->NotifyDefaultSuggestionChanged();
  }
}

// This function converts style information populated by the JSON schema
// compiler into an ACMatchClassifications object.
ACMatchClassifications StyleTypesToACMatchClassifications(
    const omnibox::SuggestResult &suggestion) {
  ACMatchClassifications match_classifications;
  if (suggestion.description_styles) {
    std::u16string description = base::UTF8ToUTF16(suggestion.description);
    std::vector<int> styles(description.length(), 0);

    for (const omnibox::MatchClassification& style :
         *suggestion.description_styles) {
      int length = style.length ? *style.length : description.length();
      size_t offset = style.offset >= 0
                          ? style.offset
                          : std::max(0, static_cast<int>(description.length()) +
                                            style.offset);

      int type_class;
      switch (style.type) {
        case omnibox::DescriptionStyleType::kUrl:
          type_class = AutocompleteMatch::ACMatchClassification::URL;
          break;
        case omnibox::DescriptionStyleType::kMatch:
          type_class = AutocompleteMatch::ACMatchClassification::MATCH;
          break;
        case omnibox::DescriptionStyleType::kDim:
          type_class = AutocompleteMatch::ACMatchClassification::DIM;
          break;
        default:
          type_class = AutocompleteMatch::ACMatchClassification::NONE;
          return match_classifications;
      }

      for (size_t j = offset; j < offset + length && j < styles.size(); ++j)
        styles[j] |= type_class;
    }

    for (size_t i = 0; i < styles.size(); ++i) {
      if (i == 0 || styles[i] != styles[i-1])
        match_classifications.push_back(
            ACMatchClassification(i, styles[i]));
    }
  } else {
    match_classifications.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
  }

  return match_classifications;
}

void ApplyDefaultSuggestionForExtensionKeyword(
    Profile* profile,
    const TemplateURL* keyword,
    const std::u16string& remaining_input,
    AutocompleteMatch* match) {
  DCHECK(keyword->type() == TemplateURL::OMNIBOX_API_EXTENSION);

  std::optional<omnibox::SuggestResult> suggestion(
      GetOmniboxDefaultSuggestion(profile, keyword->GetExtensionId()));
  if (!suggestion || suggestion->description.empty())
    return;  // fall back to the universal default

  const std::u16string kPlaceholderText(u"%s");
  const std::u16string kReplacementText(u"<input>");

  std::u16string description = base::UTF8ToUTF16(suggestion->description);
  ACMatchClassifications& description_styles = match->contents_class;
  description_styles = StyleTypesToACMatchClassifications(*suggestion);

  // Replace "%s" with the user's input and adjust the style offsets to the
  // new length of the description.
  size_t placeholder(description.find(kPlaceholderText, 0));
  if (placeholder != std::u16string::npos) {
    std::u16string replacement =
        remaining_input.empty() ? kReplacementText : remaining_input;
    description.replace(placeholder, kPlaceholderText.length(), replacement);

    for (auto& description_style : description_styles) {
      if (description_style.offset > placeholder)
        description_style.offset += replacement.length() - 2;
    }
  }

  match->contents.assign(description);
}

}  // namespace extensions
