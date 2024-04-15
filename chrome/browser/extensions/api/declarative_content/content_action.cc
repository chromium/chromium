// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <map>

#include "base/lazy_instance.h"
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
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_user_script_loader.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/script_injection_tracker.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/image_util.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/script_constants.h"
#include "third_party/skia/include/core/SkBitmap.h"
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

  ShowExtensionAction(const ShowExtensionAction&) = delete;
  ShowExtensionAction& operator=(const ShowExtensionAction&) = delete;

  ~ShowExtensionAction() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict* dict,
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
};

// Action that sets an extension's action icon.
class SetIcon : public ContentAction {
 public:
  explicit SetIcon(const gfx::Image& icon) : icon_(icon) {}

  SetIcon(const SetIcon&) = delete;
  SetIcon& operator=(const SetIcon&) = delete;

  ~SetIcon() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value::Dict* dict,
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
};

// Helper for getting JS collections into C++.
static bool AppendJSStringsToCPPStrings(const base::Value::List& append_strings,
                                        std::vector<std::string>* append_to) {
  for (const auto& entry : append_strings) {
    if (entry.is_string()) {
      append_to->push_back(entry.GetString());
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
      const base::Value::Dict* /* dict */,
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
    const base::Value::Dict* dict,
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
bool RequestContentScript::InitScriptData(const base::Value::Dict* dict,
                                          std::string* error,
                                          ScriptData* script_data) {
  const base::Value* css = dict->Find(declarative_content_constants::kCss);
  const base::Value* js = dict->Find(declarative_content_constants::kJs);

  if (!css && !js) {
    *error = base::StringPrintf(kMissingParameter, "css or js");
    return false;
  }
  if (css) {
    if (!css->is_list() || !AppendJSStringsToCPPStrings(
                               css->GetList(), &script_data->css_file_names)) {
      return false;
    }
  }
  if (js) {
    if (!js->is_list() || !AppendJSStringsToCPPStrings(
                              js->GetList(), &script_data->js_file_names)) {
      return false;
    }
  }
  if (const base::Value* all_frames_val =
          dict->Find(declarative_content_constants::kAllFrames)) {
    if (!all_frames_val->is_bool())
      return false;

    script_data->all_frames = all_frames_val->GetBool();
  }
  if (const base::Value* match_about_blank_val =
          dict->Find(declarative_content_constants::kMatchAboutBlank)) {
    if (!match_about_blank_val->is_bool())
      return false;

    script_data->match_about_blank = match_about_blank_val->GetBool();
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
  scoped_observation_.Observe(script_loader_.get());
  AddScript();
}

RequestContentScript::~RequestContentScript() {
  // This can occur either if this RequestContentScript action is removed via an
  // API call or if its extension is unloaded. If the extension is unloaded, the
  // associated `script_loader_` may have been deleted before this object which
  // means the loader has already removed `script_`.
  if (script_loader_) {
    script_loader_->RemoveScripts({script_.id()},
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
  for (const auto& css_file_name : script_data.css_file_names) {
    GURL url = extension->GetResourceURL(css_file_name);
    ExtensionResource resource = extension->GetResource(css_file_name);
    script_.css_scripts().push_back(UserScript::Content::CreateFile(
        resource.extension_root(), resource.relative_path(), url));
  }
  for (const auto& js_file_name : script_data.js_file_names) {
    GURL url = extension->GetResourceURL(js_file_name);
    ExtensionResource resource = extension->GetResource(js_file_name);
    script_.js_scripts().push_back(UserScript::Content::CreateFile(
        resource.extension_root(), resource.relative_path(), url));
  }
}

void RequestContentScript::AddScript() {
  DCHECK(script_loader_);
  UserScriptList scripts;
  scripts.push_back(UserScript::CopyMetadataFrom(script_));
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
  ScriptInjectionTracker::WillExecuteCode(base::PassKey<RequestContentScript>(),
                                          contents->GetPrimaryMainFrame(),
                                          *extension);

  mojom::LocalFrame* local_frame =
      ExtensionWebContentsObserver::GetForWebContents(contents)->GetLocalFrame(
          contents->GetPrimaryMainFrame());
  if (!local_frame) {
    // TODO(crbug.com/40763607): Need to review when this method is
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
    const base::Value::Dict* dict,
    std::string* error) {
  // We can't set a page or action's icon if the extension doesn't have one.
  if (!ActionInfo::GetExtensionActionInfo(extension)) {
    *error = kNoPageOrBrowserAction;
    return nullptr;
  }

  gfx::ImageSkia icon;
  const base::Value::Dict* canvas_set = dict->FindDict("imageData");
  if (canvas_set &&
      ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon) !=
          ExtensionAction::IconParseResult::kSuccess) {
    *error = kInvalidIconDictionary;
    return nullptr;
  }

  gfx::Image image(icon);
  const SkBitmap bitmap = image.AsBitmap();
  const bool is_sufficiently_visible =
      extensions::image_util::IsIconSufficientlyVisible(bitmap);
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
    const base::Value::Dict& json_action_dict,
    std::string* error) {
  error->clear();
  const std::string* instance_type = nullptr;
  if (!(instance_type = json_action_dict.FindString(
            declarative_content_constants::kInstanceType))) {
    *error = kMissingInstanceTypeError;
    return nullptr;
  }

  ContentActionFactory& factory = g_content_action_factory.Get();
  auto factory_method_iter = factory.factory_methods.find(*instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(browser_context, extension,
                                          &json_action_dict, error);

  *error =
      base::StringPrintf(kInvalidInstanceTypeError, instance_type->c_str());
  return nullptr;
}

// static
void ContentAction::SetAllowInvisibleIconsForTest(bool value) {
  g_allow_invisible_icons_content_action = value;
}

ContentAction::ContentAction() {}

}  // namespace extensions
