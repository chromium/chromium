// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/content_script_tracker.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/image_util.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/script_constants.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {
// Error messages.
const char kInvalidIconDictionary[] =
    "Icon dictionary must be of the form {\"19\": ImageData1, \"38\": "
    "ImageData2}";
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
const char kMissingInstanceTypeError[] = "Action is missing instanceType";
const char kMissingParameter[] = "Missing parameter is required: %s";
const char kNoAction[] =
    "Can't use declarativeContent.ShowAction without an action";
const char kNoPageOrBrowserAction[] =
    "Can't use declarativeContent.SetIcon without a page or browser action";
const char kIconNotSufficientlyVisible[] =
    "The specified icon is not sufficiently visible";

bool g_allow_invisible_icons_content_action = true;

void RecordContentActionCreated(
    declarative_content_constants::ContentActionType type) {
  base::UmaHistogramEnumeration("Extensions.DeclarativeContentActionCreated",
                                type);
}

//
// The following are concrete actions.
//

// Action that instructs to show an extension's page action.
class ShowExtensionAction : public ContentAction {
 public:
  ShowExtensionAction() {}
  ~ShowExtensionAction() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::DictionaryValue* dict,
      std::string* error) {
    // TODO(devlin): We should probably throw an error if the extension has no
    // action specified in the manifest. Currently, this is allowed since
    // extensions will have a synthesized page action.
    if (!ActionInfo::GetExtensionActionInfo(extension)) {
      *error = kNoAction;
      return nullptr;
    }

    RecordContentActionCreated(
        declarative_content_constants::ContentActionType::kShowAction);
    return std::make_unique<ShowExtensionAction>();
  }

  // Implementation of ContentAction:
  void Apply(const ApplyInfo& apply_info) const override {
    ExtensionAction* action =
        GetAction(apply_info.browser_context, apply_info.extension);
    action->DeclarativeShow(ExtensionTabUtil::GetTabId(apply_info.tab));
    ExtensionActionAPI::Get(apply_info.browser_context)->NotifyChange(
        action, apply_info.tab, apply_info.browser_context);
  }
  // The page action is already showing, so nothing needs to be done here.
  void Reapply(const ApplyInfo& apply_info) const override {}
  void Revert(const ApplyInfo& apply_info) const override {
    if (ExtensionAction* action =
            GetAction(apply_info.browser_context, apply_info.extension)) {
      action->UndoDeclarativeShow(ExtensionTabUtil::GetTabId(apply_info.tab));
      ExtensionActionAPI::Get(apply_info.browser_context)->NotifyChange(
          action, apply_info.tab, apply_info.browser_context);
    }
  }

 private:
  static ExtensionAction* GetAction(content::BrowserContext* browser_context,
                                    const Extension* extension) {
    return ExtensionActionManager::Get(browser_context)
        ->GetExtensionAction(*extension);
  }

  DISALLOW_COPY_AND_ASSIGN(ShowExtensionAction);
};

// Action that sets an extension's action icon.
class SetIcon : public ContentAction {
 public:
  explicit SetIcon(const gfx::Image& icon) : icon_(icon) {}
  ~SetIcon() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::DictionaryValue* dict,
      std::string* error);

  // Implementation of ContentAction:
  void Apply(const ApplyInfo& apply_info) const override {
    Profile* profile = Profile::FromBrowserContext(apply_info.browser_context);
    ExtensionAction* action = GetExtensionAction(profile,
                                                 apply_info.extension);
    if (action) {
      action->DeclarativeSetIcon(ExtensionTabUtil::GetTabId(apply_info.tab),
                                 apply_info.priority,
                                 icon_);
      ExtensionActionAPI::Get(profile)
          ->NotifyChange(action, apply_info.tab, profile);
    }
  }

  void Reapply(const ApplyInfo& apply_info) const override {}

  void Revert(const ApplyInfo& apply_info) const override {
    Profile* profile = Profile::FromBrowserContext(apply_info.browser_context);
    ExtensionAction* action = GetExtensionAction(profile,
                                                 apply_info.extension);
    if (action) {
      action->UndoDeclarativeSetIcon(
          ExtensionTabUtil::GetTabId(apply_info.tab),
          apply_info.priority,
          icon_);
      ExtensionActionAPI::Get(apply_info.browser_context)
          ->NotifyChange(action, apply_info.tab, profile);
    }
  }

 private:
  ExtensionAction* GetExtensionAction(Profile* profile,
                                      const Extension* extension) const {
    return ExtensionActionManager::Get(profile)->GetExtensionAction(*extension);
  }

  gfx::Image icon_;

  DISALLOW_COPY_AND_ASSIGN(SetIcon);
};

// Helper for getting JS collections into C++.
static bool AppendJSStringsToCPPStrings(const base::ListValue& append_strings,
                                        std::vector<std::string>* append_to) {
  for (auto it = append_strings.begin(); it != append_strings.end(); ++it) {
    std::string value;
    if (it->GetAsString(&value)) {
      append_to->push_back(value);
    } else {
      return false;
    }
  }

  return true;
}

struct ContentActionFactory {
  // Factory methods for ContentAction instances. |extension| is the extension
  // for which the action is being created. |dict| contains the json dictionary
  // that describes the action. |error| is used to return error messages.
  using FactoryMethod = std::unique_ptr<ContentAction> (*)(
      content::BrowserContext* /* browser_context */,
      const Extension* /* extension */,
      const base::DictionaryValue* /* dict */,
      std::string* /* error */);
  // Maps the name of a declarativeContent action type to the factory
  // function creating it.
  std::map<std::string, FactoryMethod> factory_methods;

  ContentActionFactory() {
    factory_methods[declarative_content_constants::kShowAction] =
        &ShowExtensionAction::Create;
    factory_methods[declarative_content_constants::kRequestContentScript] =
        &RequestContentScript::Create;
    factory_methods[declarative_content_constants::kSetIcon] = &SetIcon::Create;
  }
};

base::LazyInstance<ContentActionFactory>::Leaky
    g_content_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// RequestContentScript
//

struct RequestContentScript::ScriptData {
  ScriptData();
  ~ScriptData();

  std::vector<std::string> css_file_names;
  std::vector<std::string> js_file_names;
  bool all_frames;
  bool match_about_blank;
};

RequestContentScript::ScriptData::ScriptData()
    : all_frames(false),
      match_about_blank(false) {}
RequestContentScript::ScriptData::~ScriptData() {}

// static
std::unique_ptr<ContentAction> RequestContentScript::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error) {
  ScriptData script_data;
  if (!InitScriptData(dict, error, &script_data))
    return nullptr;

  RecordContentActionCreated(
      declarative_content_constants::ContentActionType::kRequestContentScript);
  return base::WrapUnique(
      new RequestContentScript(browser_context, extension, script_data));
}

// static
bool RequestContentScript::InitScriptData(const base::DictionaryValue* dict,
                                          std::string* error,
                                          ScriptData* script_data) {
  const base::ListValue* list_value = NULL;

  if (!dict->HasKey(declarative_content_constants::kCss) &&
      !dict->HasKey(declarative_content_constants::kJs)) {
    *error = base::StringPrintf(kMissingParameter, "css or js");
    return false;
  }
  if (dict->HasKey(declarative_content_constants::kCss)) {
    if (!dict->GetList(declarative_content_constants::kCss, &list_value) ||
        !AppendJSStringsToCPPStrings(*list_value,
                                     &script_data->css_file_names)) {
      return false;
    }
  }
  if (dict->HasKey(declarative_content_constants::kJs)) {
    if (!dict->GetList(declarative_content_constants::kJs, &list_value) ||
        !AppendJSStringsToCPPStrings(*list_value,
                                     &script_data->js_file_names)) {
      return false;
    }
  }
  if (dict->HasKey(declarative_content_constants::kAllFrames)) {
    if (!dict->GetBoolean(declarative_content_constants::kAllFrames,
                          &script_data->all_frames))
      return false;
  }
  if (dict->HasKey(declarative_content_constants::kMatchAboutBlank)) {
    if (!dict->GetBoolean(declarative_content_constants::kMatchAboutBlank,
                          &script_data->match_about_blank)) {
      return false;
    }
  }

  return true;
}

RequestContentScript::RequestContentScript(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const ScriptData& script_data) {
  mojom::HostID host_id(mojom::HostID::HostType::kExtensions, extension->id());
  InitScript(host_id, extension, script_data);

  script_loader_ = ExtensionSystem::Get(browser_context)
                       ->user_script_manager()
                       ->GetUserScriptLoaderForExtension(extension->id());
  scoped_observation_.Observe(script_loader_);
  AddScript();
}

RequestContentScript::~RequestContentScript() {
  // This can occur either if this RequestContentScript action is removed via an
  // API call or if its extension is unloaded. If the extension is unloaded, the
  // associated `script_loader_` may have been deleted before this object which
  // means the loader has already removed `script_`.
  if (script_loader_) {
    script_loader_->RemoveScripts(
        {UserScriptIDPair(script_.id(), script_.host_id())},
        UserScriptLoader::ScriptsLoadedCallback());
  }
}

void RequestContentScript::InitScript(const mojom::HostID& host_id,
                                      const Extension* extension,
                                      const ScriptData& script_data) {
  script_.set_id(UserScript::GenerateUserScriptID());
  script_.set_host_id(host_id);
  script_.set_run_location(mojom::RunLocation::kBrowserDriven);
  script_.set_match_all_frames(script_data.all_frames);
  script_.set_match_origin_as_fallback(
      script_data.match_about_blank
          ? MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree
          : MatchOriginAsFallbackBehavior::kNever);
  for (auto it = script_data.css_file_names.cbegin();
       it != script_data.css_file_names.cend(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.css_scripts().push_back(std::make_unique<UserScript::File>(
        resource.extension_root(), resource.relative_path(), url));
  }
  for (auto it = script_data.js_file_names.cbegin();
       it != script_data.js_file_names.cend(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.js_scripts().push_back(std::make_unique<UserScript::File>(
        resource.extension_root(), resource.relative_path(), url));
  }
}

void RequestContentScript::AddScript() {
  DCHECK(script_loader_);
  auto scripts = std::make_unique<UserScriptList>();
  scripts->push_back(UserScript::CopyMetadataFrom(script_));
  script_loader_->AddScripts(std::move(scripts),
                             UserScriptLoader::ScriptsLoadedCallback());
}

void RequestContentScript::Apply(const ApplyInfo& apply_info) const {
  InstructRenderProcessToInject(apply_info.tab, apply_info.extension);
}

void RequestContentScript::Reapply(const ApplyInfo& apply_info) const {
  InstructRenderProcessToInject(apply_info.tab, apply_info.extension);
}

void RequestContentScript::Revert(const ApplyInfo& apply_info) const {}

void RequestContentScript::InstructRenderProcessToInject(
    content::WebContents* contents,
    const Extension* extension) const {
  ContentScriptTracker::WillExecuteCode(base::PassKey<RequestContentScript>(),
                                        contents->GetMainFrame(), *extension);

  mojom::LocalFrame* local_frame =
      ExtensionWebContentsObserver::GetForWebContents(contents)->GetLocalFrame(
          contents->GetMainFrame());
  if (!local_frame) {
    // TODO(https://crbug.com/1203579): Need to review when this method is
    // called with non-live frame.
    return;
  }
  local_frame->ExecuteDeclarativeScript(
      sessions::SessionTabHelper::IdForTab(contents).id(), extension->id(),
      script_.id(), contents->GetLastCommittedURL());
}

void RequestContentScript::OnScriptsLoaded(
    UserScriptLoader* loader,
    content::BrowserContext* browser_context) {}

void RequestContentScript::OnUserScriptLoaderDestroyed(
    UserScriptLoader* loader) {
  DCHECK_EQ(script_loader_, loader);
  scoped_observation_.Reset();
  script_loader_ = nullptr;
}

// static
std::unique_ptr<ContentAction> SetIcon::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error) {
  // We can't set a page or action's icon if the extension doesn't have one.
  if (!ActionInfo::GetExtensionActionInfo(extension)) {
    *error = kNoPageOrBrowserAction;
    return nullptr;
  }

  gfx::ImageSkia icon;
  const base::DictionaryValue* canvas_set = NULL;
  if (dict->GetDictionary("imageData", &canvas_set) &&
      ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon) !=
          ExtensionAction::IconParseResult::kSuccess) {
    *error = kInvalidIconDictionary;
    return nullptr;
  }

  gfx::Image image(icon);
  const SkBitmap bitmap = image.AsBitmap();
  const bool is_sufficiently_visible =
      extensions::image_util::IsIconSufficientlyVisible(bitmap);
  base::UmaHistogramBoolean("Extensions.DeclarativeSetIconWasVisible",
                            is_sufficiently_visible);
  const bool is_sufficiently_visible_rendered =
      extensions::ui_util::IsRenderedIconSufficientlyVisibleForBrowserContext(
          bitmap, browser_context);
  base::UmaHistogramBoolean("Extensions.DeclarativeSetIconWasVisibleRendered",
                            is_sufficiently_visible_rendered);
  if (!is_sufficiently_visible && !g_allow_invisible_icons_content_action) {
    *error = kIconNotSufficientlyVisible;
    return nullptr;
  }

  RecordContentActionCreated(
      declarative_content_constants::ContentActionType::kSetIcon);
  return std::make_unique<SetIcon>(image);
}

//
// ContentAction
//

ContentAction::~ContentAction() {}

// static
std::unique_ptr<ContentAction> ContentAction::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error) {
  error->clear();
  const base::DictionaryValue* action_dict = NULL;
  std::string instance_type;
  if (!(json_action.GetAsDictionary(&action_dict) &&
        action_dict->GetString(declarative_content_constants::kInstanceType,
                               &instance_type))) {
    *error = kMissingInstanceTypeError;
    return nullptr;
  }

  ContentActionFactory& factory = g_content_action_factory.Get();
  auto factory_method_iter = factory.factory_methods.find(instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(
        browser_context, extension, action_dict, error);

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return nullptr;
}

// static
void ContentAction::SetAllowInvisibleIconsForTest(bool value) {
  g_allow_invisible_icons_content_action = value;
}

ContentAction::ContentAction() {}

}  // namespace extensions
