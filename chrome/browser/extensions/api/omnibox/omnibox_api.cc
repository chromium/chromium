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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "extensions/browser/icon_util.h"
#include "extensions/browser/install_prefs_helper.h"
#include "extensions/browser/permissions/active_tab_permission_granter.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

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

bool IsUnscopedModeAllowed(const Extension* extension) {
  // The extension can use unscoepd mode if the feature is enabled and the
  // permission has been granted.
  return base::FeatureList::IsEnabled(
             extensions_features::kExperimentalOmniboxLabs) &&
         extension->permissions_data()->HasAPIPermission(
             mojom::APIPermissionID::kOmniboxDirectInput);
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
  extensions::ActiveTabPermissionGranter::FromWebContents(web_contents)
      ->GrantIfRequested(extension);

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
  event->user_gesture = EventRouter::UserGestureState::kEnabled;
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

// static
void ExtensionOmniboxEventRouter::OnActionExecuted(
    Profile* profile,
    const ExtensionId& extension_id,
    const std::string& action_name,
    const std::string& content) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!event_router->ExtensionHasEventListener(
          extension_id, omnibox::OnActionExecuted::kEventName)) {
    return;
  }

  omnibox::ActionExecution action_execution;
  action_execution.action_name = action_name;
  action_execution.content = content;
  auto event = std::make_unique<Event>(
      events::OMNIBOX_ON_ACTION_EXECUTED, omnibox::OnActionExecuted::kEventName,
      omnibox::OnActionExecuted::Create(std::move(action_execution)), profile);
  event->user_gesture = EventRouter::UserGestureState::kEnabled;
  event_router->DispatchEventToExtension(extension_id, std::move(event));
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

  permissions_manager_observation_.Observe(PermissionsManager::Get(profile_));
}

void OmniboxAPI::Shutdown() {
  template_url_subscription_ = {};
  permissions_manager_observation_.Reset();
}

OmniboxAPI::~OmniboxAPI() = default;

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
        url_service_->RegisterExtensionControlledTURL(
            extension->id(), extension->short_name(), keyword,
            GetTemplateURLStringForExtension(extension->id()),
            GetLastUpdateTime(ExtensionPrefs::Get(profile_), extension->id()),
            IsUnscopedModeAllowed(extension));
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

void OmniboxAPI::OnExtensionPermissionsUpdated(
    const Extension& extension,
    const PermissionSet& permissions,
    PermissionsManager::UpdateReason reason) {
  if (!permissions.HasAPIPermission(
          mojom::APIPermissionID::kOmniboxDirectInput)) {
    return;
  }

  if (reason == PermissionsManager::UpdateReason::kAdded &&
      base::FeatureList::IsEnabled(
          extensions_features::kExperimentalOmniboxLabs)) {
    url_service_->AddToUnscopedModeExtensionIds(extension.id());
  } else if (reason == PermissionsManager::UpdateReason::kRemoved) {
    url_service_->RemoveFromUnscopedModeExtensionIdsIfPresent(extension.id());
  }
}

gfx::Image OmniboxAPI::GetOmniboxIcon(const ExtensionId& extension_id) {
  return omnibox_icon_manager_.GetIcon(extension_id);
}

void OmniboxAPI::OnTemplateURLsLoaded() {
  // Register keywords for pending extensions.
  template_url_subscription_ = {};
  for (const Extension* i : pending_extensions_) {
    url_service_->RegisterExtensionControlledTURL(
        i->id(), i->short_name(), OmniboxInfo::GetKeyword(i),
        GetTemplateURLStringForExtension(i->id()),
        GetLastUpdateTime(ExtensionPrefs::Get(profile_), i->id()),
        IsUnscopedModeAllowed(i));
  }
  pending_extensions_.clear();
}

template <>
void BrowserContextKeyedAPIFactory<OmniboxAPI>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(PermissionsManager::GetFactory());
}

OmniboxSendSuggestionsFunction::OmniboxSendSuggestionsFunction() = default;
OmniboxSendSuggestionsFunction::~OmniboxSendSuggestionsFunction() = default;

ExtensionFunction::ResponseAction OmniboxSendSuggestionsFunction::Run() {
  std::optional<api::omnibox::SendSuggestions::Params> params =
      SendSuggestions::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  request_id_ = params->request_id;

  if (!params->suggest_results.empty()) {
    std::vector<std::string_view> inputs;
    inputs.reserve(params->suggest_results.size());
    for (const auto& suggestion : params->suggest_results) {
      std::vector<ExtensionSuggestion::Action> actions;
      inputs.push_back(suggestion.description);
      if (suggestion.actions) {
        if (!IsUnscopedModeAllowed(extension())) {
          return RespondNow(
              Error(ExtensionOmniboxEventRouter::
                        kActionsRequireDirectInputPermissionError));
        }
        if (suggestion.actions->size() >
            ExtensionOmniboxEventRouter::kMaxSuggestionActions) {
          return RespondNow(Error(base::StringPrintf(
              ExtensionOmniboxEventRouter::kMaxSuggestionActionsExceededError,
              suggestion.actions->size(),
              ExtensionOmniboxEventRouter::kMaxSuggestionActions)));
        }
        actions.reserve(suggestion.actions->size());
        for (const auto& action : *suggestion.actions) {
          base::Value::Dict canvas_set =
              action.icon ? action.icon->ToValue() : base::Value::Dict();
          gfx::ImageSkia image_skia;
          if (!canvas_set.empty()) {
            base::Value::Dict& image_data = *canvas_set.FindDict("imageData");
            // The image data should have been verified by the pre-validation
            // param update.
            CHECK(!image_data.empty());
            // TODO(crbug.com/408069174): Move ParseIconFromCanvasDictionary
            // outside `ExtensionAction` into a common file.
            if (extensions::ParseIconFromCanvasDictionary(image_data,
                                                          &image_skia) !=
                extensions::IconParseResult::kSuccess) {
              return RespondNow(Error(base::StringPrintf(
                  ExtensionOmniboxEventRouter::kActionIconError,
                  suggestion.description, action.name)));
            }
          }
          actions.emplace_back(action.name, action.label, action.tooltip_text,
                               gfx::Image(image_skia));
        }
      }

      const std::vector<api::omnibox::MatchClassification> empty_styles;
      const std::vector<api::omnibox::MatchClassification>* styles_ptr =
          suggestion.description_styles ? &suggestion.description_styles.value()
                                        : &empty_styles;
      extension_suggestions_.emplace_back(
          suggestion.content, suggestion.description,
          suggestion.deletable.value_or(false),
          StyleTypesToACMatchClassifications(styles_ptr,
                                             suggestion.description),
          std::move(actions), suggestion.icon_url);
    }

    if (is_from_service_worker()) {
      ParseDescriptionsAndStyles(
          inputs,
          base::BindOnce(
              &OmniboxSendSuggestionsFunction::OnParsedDescriptionsAndStyles,
              this));
      return RespondLater();
    }
  }

  NotifySuggestionsReady();
  return RespondNow(NoArguments());
}

void OmniboxSendSuggestionsFunction::OnParsedDescriptionsAndStyles(
    DescriptionAndStylesResult result) {
  DCHECK_NE(0u, extension_suggestions_.size());
  // Since the XML parsing happens asynchronously, the browser context can be
  // torn down in the interim. If this happens, early-out.
  if (!browser_context()) {
    return;
  }

  if (!result.error.empty()) {
    Respond(Error(std::move(result.error)));
    return;
  }

  if (result.descriptions_and_styles.size() != extension_suggestions_.size()) {
    // This can technically happen if the extension provided input that mucked
    // with our XML parsing (see suggestion_parser_unittest.cc). This isn't a
    // security concern, but would mean that our mapping to record the other
    // fields in the suggestion are mismatched. Abort. Since there's no
    // legitimate case for this happening, just emit a generic error message.
    Respond(Error("Invalid input."));
    return;
  }

  for (size_t i = 0; i < extension_suggestions_.size(); ++i) {
    extension_suggestions_[i].description =
        base::UTF16ToUTF8(result.descriptions_and_styles[i].description);
    extension_suggestions_[i].match_classifications =
        StyleTypesToACMatchClassifications(
            &result.descriptions_and_styles[i].styles,
            extension_suggestions_[i].description);
  }

  NotifySuggestionsReady();
  Respond(NoArguments());
}

void OmniboxSendSuggestionsFunction::NotifySuggestionsReady() {
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  OmniboxSuggestionsWatcherFactory::GetForBrowserContext(profile)
      ->NotifySuggestionsReady(extension_suggestions_, request_id_,
                               extension_id());
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
    const std::vector<omnibox::MatchClassification>* description_styles,
    const std::string& suggestion_description) {
  ACMatchClassifications match_classifications;
  if (!description_styles->empty()) {
    std::u16string description = base::UTF8ToUTF16(suggestion_description);
    std::vector<int> styles(description.length(), 0);

    for (const omnibox::MatchClassification& style : *description_styles) {
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

  const std::vector<api::omnibox::MatchClassification> empty_styles;
  const std::vector<api::omnibox::MatchClassification>* styles_list =
      suggestion->description_styles ? &suggestion->description_styles.value()
                                     : &empty_styles;
  description_styles =
      StyleTypesToACMatchClassifications(styles_list, suggestion->description);

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
