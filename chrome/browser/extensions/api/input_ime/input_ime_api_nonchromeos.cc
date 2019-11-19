// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is for non-chromeos (win & linux) functions, such as
// chrome.input.ime.activate, chrome.input.ime.createWindow and
// chrome.input.ime.onSelectionChanged.
// TODO(azurewei): May refactor the code structure by using delegate or
// redesign the API to remove this platform-specific file in the future.

#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/input_method/input_method_engine.h"
#include "chrome/common/extensions/api/input_ime.h"
#include "extensions/browser/extension_prefs.h"
#include "ui/base/ime/ime_bridge.h"
#include "ui/gfx/geometry/rect.h"

namespace input_ime = extensions::api::input_ime;
namespace OnCompositionBoundsChanged =
    extensions::api::input_ime::OnCompositionBoundsChanged;
using ui::IMEEngineHandlerInterface;
using input_method::InputMethodEngine;
using input_method::InputMethodEngineBase;

namespace input_ime = extensions::api::input_ime;

namespace {

const char kErrorNoActiveEngine[] = "The extension has not been activated.";
const char kErrorPermissionDenied[] = "User denied permission.";
const char kErrorCouldNotFindActiveBrowser[] =
    "Cannot find the active browser.";
const char kErrorNotCalledFromUserAction[] =
    "This API is only allowed to be called from a user action.";

// A preference determining whether to hide the warning bubble next time.
// Not used for now.
const char kPrefWarningBubbleNeverShow[] = "skip_ime_warning_bubble";

// A preference to see whether the API has never been called, or it's the first
// time to call since loaded the extension.
// This is used from make an exception for user_gesture checking: no need the
// check when restarting chrome.
const char kPrefNeverActivatedSinceLoaded[] = "never_activated_since_loaded";

// A preference to see whether the extension is the last active extension.
const char kPrefLastActiveEngine[] = "last_activated_ime_engine";

class ImeBridgeObserver : public ui::IMEBridgeObserver {
 public:
  void OnRequestSwitchEngine() override {
    Browser* browser = chrome::FindLastActive();
    if (!browser)
      return;
    extensions::InputImeEventRouter* router =
        extensions::GetInputImeEventRouter(browser->profile());
    if (!router)
      return;
    ui::IMEBridge::Get()->SetCurrentEngineHandler(router->active_engine());
  }
  void OnInputContextHandlerChanged() override {}
};

class ImeObserverNonChromeOS : public ui::ImeObserver {
 public:
  ImeObserverNonChromeOS(const std::string& extension_id, Profile* profile)
      : ImeObserver(extension_id, profile) {}

  ~ImeObserverNonChromeOS() override = default;

  void OnCompositionBoundsChanged(
      const std::vector<gfx::Rect>& bounds) override {
    if (extension_id_.empty() || bounds.empty() ||
        !HasListener(OnCompositionBoundsChanged::kEventName))
      return;

    std::vector<input_ime::Bounds> bounds_list;
    for (const auto& bound : bounds) {
      input_ime::Bounds bounds_value;
      bounds_value.left = bound.x();
      bounds_value.top = bound.y();
      bounds_value.width = bound.width();
      bounds_value.height = bound.height();
      bounds_list.push_back(std::move(bounds_value));
    }

    std::unique_ptr<base::ListValue> args(
        OnCompositionBoundsChanged::Create(bounds_list));

    DispatchEventToExtension(
        extensions::events::INPUT_IME_ON_COMPOSITION_BOUNDS_CHANGED,
        OnCompositionBoundsChanged::kEventName, std::move(args));
  }

 private:
  // ImeObserver overrides.
  void DispatchEventToExtension(
      extensions::events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args) override {
    auto event = std::make_unique<extensions::Event>(
        histogram_value, event_name, std::move(args), profile_);
    extensions::EventRouter::Get(profile_)
        ->DispatchEventToExtension(extension_id_, std::move(event));
  }

  std::string GetCurrentScreenType() override { return "normal"; }

  DISALLOW_COPY_AND_ASSIGN(ImeObserverNonChromeOS);
};

}  // namespace

namespace extensions {

InputMethodEngine* GetEngineIfActive(content::BrowserContext* browser_context,
                                     const std::string& extension_id) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  InputMethodEngine* engine =
      event_router ? static_cast<InputMethodEngine*>(
                         event_router->GetEngineIfActive(extension_id))
                   : nullptr;
  return engine;
}

void InputImeAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  // No-op if called multiple times.
  ui::IMEBridge::Initialize();
  if (!observer_) {
    observer_ = std::make_unique<ImeBridgeObserver>();
    ui::IMEBridge::Get()->AddObserver(observer_.get());
  }

  // Set the preference kPrefNeverActivatedSinceLoaded true to indicate
  // input.ime.activate API has been never called since loaded.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  ExtensionPrefs::Get(profile)->UpdateExtensionPref(
      extension->id(), kPrefNeverActivatedSinceLoaded,
      std::make_unique<base::Value>(true));
}

void InputImeAPI::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  InputImeEventRouter* event_router =
      GetInputImeEventRouter(Profile::FromBrowserContext(browser_context));
  if (event_router) {
    // Records the extension is not the last active IME engine.
    ExtensionPrefs::Get(Profile::FromBrowserContext(browser_context))
        ->UpdateExtensionPref(extension->id(), kPrefLastActiveEngine,
                              std::make_unique<base::Value>(false));
    event_router->DeleteInputMethodEngine(extension->id());
  }
}

void InputImeAPI::OnListenerAdded(const EventListenerInfo& details) {}

InputImeEventRouter::InputImeEventRouter(Profile* profile)
    : InputImeEventRouterBase(profile), active_engine_(nullptr) {}

InputImeEventRouter::~InputImeEventRouter() {
  if (active_engine_)
    DeleteInputMethodEngine(active_engine_->GetExtensionId());
}

InputMethodEngineBase* InputImeEventRouter::GetEngineIfActive(
    const std::string& extension_id) {
  return (ui::IMEBridge::Get()->GetCurrentEngineHandler() &&
          active_engine_ &&
          active_engine_->GetExtensionId() == extension_id)
             ? active_engine_
             : nullptr;
}

void InputImeEventRouter::SetActiveEngine(const std::string& extension_id) {
  // Records the extension is the last active IME engine.
  ExtensionPrefs::Get(GetProfile())
      ->UpdateExtensionPref(extension_id, kPrefLastActiveEngine,
                            std::make_unique<base::Value>(true));
  if (active_engine_) {
    if (active_engine_->GetExtensionId() == extension_id) {
      active_engine_->Enable(std::string());
      ui::IMEBridge::Get()->SetCurrentEngineHandler(active_engine_);
      return;
    }
    // Records the extension is not the last active IME engine.
    ExtensionPrefs::Get(GetProfile())
        ->UpdateExtensionPref(active_engine_->GetExtensionId(),
                              kPrefLastActiveEngine,
                              std::make_unique<base::Value>(false));
    DeleteInputMethodEngine(active_engine_->GetExtensionId());
  }

  std::unique_ptr<input_method::InputMethodEngine> engine(
      new input_method::InputMethodEngine());
  std::unique_ptr<InputMethodEngineBase::Observer> observer(
      new ImeObserverNonChromeOS(extension_id, GetProfile()));
  engine->Initialize(std::move(observer), extension_id.c_str(), GetProfile());
  engine->Enable(std::string());
  active_engine_ = engine.release();
  ui::IMEBridge::Get()->SetCurrentEngineHandler(active_engine_);
}

void InputImeEventRouter::DeleteInputMethodEngine(
    const std::string& extension_id) {
  if (active_engine_ && active_engine_->GetExtensionId() == extension_id) {
    active_engine_->Disable();
    ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
    delete active_engine_;
    active_engine_ = nullptr;
  }
}

// static
bool InputImeActivateFunction::disable_bubble_for_testing_ = false;

ExtensionFunction::ResponseAction InputImeActivateFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  if (!event_router)
    return RespondNow(Error(kErrorNoActiveEngine));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);

  bool never_activated_since_loaded = false;
  bool last_active_ime_engine = false;

  if (prefs->ReadPrefAsBoolean(extension_id(), kPrefNeverActivatedSinceLoaded,
                               &never_activated_since_loaded) &&
      never_activated_since_loaded &&
      prefs->ReadPrefAsBoolean(extension_id(), kPrefLastActiveEngine,
                               &last_active_ime_engine) &&
      last_active_ime_engine) {
    // If the extension is the last active IME engine, and the API is called at
    // loading the extension, we can tell the API is called from restarting
    // chrome. No need for user gesture checking.
    event_router->SetActiveEngine(extension_id());
    ExtensionPrefs::Get(profile)->UpdateExtensionPref(
        extension_id(), kPrefNeverActivatedSinceLoaded,
        std::make_unique<base::Value>(false));
    return RespondNow(NoArguments());
  }
  // The API has already been called at least once.
  ExtensionPrefs::Get(profile)->UpdateExtensionPref(
      extension_id(), kPrefNeverActivatedSinceLoaded,
      std::make_unique<base::Value>(false));

  // Otherwise, this API is only allowed to be called from a user action.
  if (!user_gesture())
    return RespondNow(Error(kErrorNotCalledFromUserAction));

  // Disable using the warning bubble for testing.
  if (disable_bubble_for_testing_) {
    event_router->SetActiveEngine(extension_id());
    return RespondNow(NoArguments());
  }

  // Disables the warning bubble since we don't need run-time checking anymore.
  bool warning_bubble_never_show = true;
  if (warning_bubble_never_show) {
    // If user allows to activate the extension without showing the warning
    // bubble, sets the active engine directly.
    // Otherwise, the extension will be activated when the user presses the 'OK'
    // button on the warning bubble.
    event_router->SetActiveEngine(extension_id());
    return RespondNow(NoArguments());
  }

  // TODO(azurewei): Remove the warning bubble related codes.
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser)
    return RespondNow(Error(kErrorCouldNotFindActiveBrowser));

  // Creates and shows the warning bubble. The ImeWarningBubble is self-owned,
  // it deletes itself when closed.
  browser->window()->ShowImeWarningBubble(
      extension(),
      base::Bind(&InputImeActivateFunction::OnPermissionBubbleFinished, this));
  return RespondLater();
}

void InputImeActivateFunction::OnPermissionBubbleFinished(
    ImeWarningBubblePermissionStatus status) {
  if (status == ImeWarningBubblePermissionStatus::DENIED ||
      status == ImeWarningBubblePermissionStatus::ABORTED) {
    // Fails to activate the extension.
    Respond(Error(kErrorPermissionDenied));
    return;
  }

  DCHECK(status == ImeWarningBubblePermissionStatus::GRANTED ||
         status == ImeWarningBubblePermissionStatus::GRANTED_AND_NEVER_SHOW);

  // Activates this extension if user presses the 'OK' button.
  Profile* profile = Profile::FromBrowserContext(browser_context());
  InputImeEventRouter* event_router = GetInputImeEventRouter(profile);
  if (!event_router) {
    Respond(Error(kErrorNoActiveEngine));
    return;
  }
  event_router->SetActiveEngine(extension_id());

  if (status == ImeWarningBubblePermissionStatus::GRANTED_AND_NEVER_SHOW) {
    // Updates the extension preference if user checks the 'Never show this
    // again' check box. So we can activate the extension directly next time.
    ExtensionPrefs::Get(profile)->UpdateExtensionPref(
        extension_id(), kPrefWarningBubbleNeverShow,
        std::make_unique<base::Value>(true));
  }

  Respond(NoArguments());
}

ExtensionFunction::ResponseAction InputImeDeactivateFunction::Run() {
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id());
  ui::IMEBridge::Get()->SetCurrentEngineHandler(nullptr);
  if (engine)
    engine->CloseImeWindows();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeCreateWindowFunction::Run() {
  // Using input_ime::CreateWindow::Params::Create() causes the link errors on
  // Windows, only if the method name is 'createWindow'.
  // So doing the by-hand parameter unpacking here.
  // TODO(shuchen,rdevlin.cronin): investigate the root cause for the link
  // errors.
  const base::DictionaryValue* params = nullptr;
  args_->GetDictionary(0, &params);
  EXTENSION_FUNCTION_VALIDATE(params);
  input_ime::CreateWindowOptions options;
  input_ime::CreateWindowOptions::Populate(*params, &options);

  gfx::Rect bounds(0, 0, 100, 100);  // Default bounds.
  if (options.bounds.get()) {
    bounds.set_x(options.bounds->left);
    bounds.set_y(options.bounds->top);
    bounds.set_width(options.bounds->width);
    bounds.set_height(options.bounds->height);
  }

  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  std::string error;
  int frame_id = engine->CreateImeWindow(
      extension(), render_frame_host(),
      options.url.get() ? *options.url : url::kAboutBlankURL,
      options.window_type == input_ime::WINDOW_TYPE_FOLLOWCURSOR
          ? ui::ImeWindow::FOLLOW_CURSOR
          : ui::ImeWindow::NORMAL,
      bounds, &error);
  if (!frame_id)
    return RespondNow(Error(error));

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set("frameId", std::make_unique<base::Value>(frame_id));

  return RespondNow(OneArgument(std::move(result)));
}

ExtensionFunction::ResponseAction InputImeShowWindowFunction::Run() {
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  std::unique_ptr<api::input_ime::ShowWindow::Params> params(
      api::input_ime::ShowWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  engine->ShowImeWindow(params->window_id);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction InputImeHideWindowFunction::Run() {
  InputMethodEngine* engine =
      GetEngineIfActive(browser_context(), extension_id());
  if (!engine)
    return RespondNow(Error(kErrorNoActiveEngine));

  std::unique_ptr<api::input_ime::HideWindow::Params> params(
      api::input_ime::HideWindow::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  engine->HideImeWindow(params->window_id);
  return RespondNow(NoArguments());
}

}  // namespace extensions
