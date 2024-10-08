// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "aida_client.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_eye_dropper.h"
#include "chrome/browser/devtools/features.h"
#include "chrome/browser/devtools/process_sharing_infobar_delegate.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/webui/devtools/devtools_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/language/core/browser/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "net/cert/x509_certificate.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
#include "base/win/windows_h_disallowed.h"
#endif  // defined(_WINDOWS_)

using blink::WebInputEvent;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_devtools_window_instances =
    LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<
    std::vector<base::RepeatingCallback<void(DevToolsWindow*)>>>::Leaky
    g_creation_callbacks = LAZY_INSTANCE_INITIALIZER;

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";
static const char kDefaultFrontendURL[] =
    "devtools://devtools/bundled/devtools_app.html";
static const char kNodeFrontendURL[] =
    "devtools://devtools/bundled/node_app.html";
static const char kWorkerFrontendURL[] =
    "devtools://devtools/bundled/worker_app.html";
static const char kJSFrontendURL[] = "devtools://devtools/bundled/js_app.html";
static const char kFallbackFrontendURL[] =
    "devtools://devtools/bundled/inspector.html";

bool FindInspectedBrowserAndTabIndex(
    WebContents* inspected_web_contents, Browser** browser, int* tab) {
  if (!inspected_web_contents)
    return false;

  for (Browser* b : *BrowserList::GetInstance()) {
    int tab_index =
        b->tab_strip_model()->GetIndexOfWebContents(inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = b;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

void SetPreferencesFromJson(Profile* profile, const std::string& json) {
  std::optional<base::Value> parsed = base::JSONReader::Read(json);
  if (!parsed || !parsed->is_dict())
    return;
  ScopedDictPrefUpdate update(profile->GetPrefs(), prefs::kDevToolsPreferences);
  for (auto dict_value : parsed->GetDict()) {
    if (!dict_value.second.is_string())
      continue;
    update->Set(dict_value.first, std::move(dict_value.second));
  }
}

// DevToolsToolboxDelegate ----------------------------------------------------

class DevToolsToolboxDelegate
    : public content::WebContentsObserver,
      public content::WebContentsDelegate {
 public:
  DevToolsToolboxDelegate(WebContents* toolbox_contents,
                          WebContents* inspected_web_contents);

  DevToolsToolboxDelegate(const DevToolsToolboxDelegate&) = delete;
  DevToolsToolboxDelegate& operator=(const DevToolsToolboxDelegate&) = delete;

  ~DevToolsToolboxDelegate() override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void WebContentsDestroyed() override;

 private:
  BrowserWindow* GetInspectedBrowserWindow();
  base::WeakPtr<content::WebContents> inspected_web_contents_;
};

DevToolsToolboxDelegate::DevToolsToolboxDelegate(WebContents* toolbox_contents,
                                                 WebContents* web_contents)
    : WebContentsObserver(toolbox_contents),
      inspected_web_contents_(web_contents ? web_contents->GetWeakPtr()
                                           : nullptr) {}

DevToolsToolboxDelegate::~DevToolsToolboxDelegate() {
}

content::WebContents* DevToolsToolboxDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  DCHECK(source == web_contents());
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    return nullptr;
  }
  base::WeakPtr<content::NavigationHandle> navigation_handle =
      source->GetController().LoadURLWithParams(
          content::NavigationController::LoadURLParams(params));

  if (navigation_handle_callback && navigation_handle) {
    std::move(navigation_handle_callback).Run(*navigation_handle);
  }
  return source;
}

content::KeyboardEventProcessingResult
DevToolsToolboxDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->PreHandleKeyboardEvent(event);
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsToolboxDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return false;
  }
  BrowserWindow* window = GetInspectedBrowserWindow();
  return window && window->HandleKeyboardEvent(event);
}

void DevToolsToolboxDelegate::WebContentsDestroyed() {
  delete this;
}

BrowserWindow* DevToolsToolboxDelegate::GetInspectedBrowserWindow() {
  if (!inspected_web_contents_)
    return nullptr;
  Browser* browser = nullptr;
  int tab = 0;
  if (FindInspectedBrowserAndTabIndex(inspected_web_contents_.get(), &browser,
                                      &tab))
    return browser->window();
  return nullptr;
}

// static
GURL DecorateFrontendURL(const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kDevToolsFlags)) {
    frontend_url = frontend_url +
                   ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
                   command_line->GetSwitchValueASCII(switches::kDevToolsFlags);
  }

  if (command_line->HasSwitch(switches::kCustomDevtoolsFrontend)) {
    frontend_url = frontend_url +
                   ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
                   "debugFrontend=true";
  }

  return GURL(frontend_url);
}

}  // namespace

// DevToolsEventForwarder -----------------------------------------------------

class DevToolsEventForwarder {
 public:
  explicit DevToolsEventForwarder(DevToolsWindow* window)
     : devtools_window_(window) {}

  DevToolsEventForwarder(const DevToolsEventForwarder&) = delete;
  DevToolsEventForwarder& operator=(const DevToolsEventForwarder&) = delete;

  // Registers whitelisted shortcuts with the forwarder.
  // Only registered keys will be forwarded to the DevTools frontend.
  void SetWhitelistedShortcuts(const std::string& message);

  // Forwards a keyboard event to the DevTools frontend if it is whitelisted.
  // Returns |true| if the event has been forwarded, |false| otherwise.
  bool ForwardEvent(const input::NativeWebKeyboardEvent& event);

 private:
  static bool KeyWhitelistingAllowed(int key_code, int modifiers);
  static int CombineKeyCodeAndModifiers(int key_code, int modifiers);

  raw_ptr<DevToolsWindow> devtools_window_;
  std::set<int> whitelisted_keys_;
};

void DevToolsEventForwarder::SetWhitelistedShortcuts(
    const std::string& message) {
  std::optional<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message || !parsed_message->is_list())
    return;
  for (const auto& list_item : parsed_message->GetList()) {
    if (!list_item.is_dict())
      continue;
    int key_code = list_item.GetDict().FindInt("keyCode").value_or(0);
    if (key_code == 0)
      continue;
    int modifiers = list_item.GetDict().FindInt("modifiers").value_or(0);
    if (!KeyWhitelistingAllowed(key_code, modifiers)) {
      LOG(WARNING) << "Key whitelisting forbidden: "
                   << "(" << key_code << "," << modifiers << ")";
      continue;
    }
    whitelisted_keys_.insert(CombineKeyCodeAndModifiers(key_code, modifiers));
  }
}

bool DevToolsEventForwarder::ForwardEvent(
    const input::NativeWebKeyboardEvent& event) {
  std::string event_type;
  switch (event.GetType()) {
    case WebInputEvent::Type::kKeyDown:
    case WebInputEvent::Type::kRawKeyDown:
      event_type = kKeyDownEventName;
      break;
    case WebInputEvent::Type::kKeyUp:
      event_type = kKeyUpEventName;
      break;
    default:
      return false;
  }

  int key_code = ui::LocatedToNonLocatedKeyboardCode(
      static_cast<ui::KeyboardCode>(event.windows_key_code));
  int modifiers = event.GetModifiers() &
                  (WebInputEvent::kShiftKey | WebInputEvent::kControlKey |
                   WebInputEvent::kAltKey | WebInputEvent::kMetaKey);
  int key = CombineKeyCodeAndModifiers(key_code, modifiers);
  if (whitelisted_keys_.find(key) == whitelisted_keys_.end())
    return false;

  base::Value::Dict event_data;
  event_data.Set("type", event_type);
  event_data.Set("key", ui::KeycodeConverter::DomKeyToKeyString(
                            static_cast<ui::DomKey>(event.dom_key)));
  event_data.Set("code", ui::KeycodeConverter::DomCodeToCodeString(
                             static_cast<ui::DomCode>(event.dom_code)));
  event_data.Set("keyCode", key_code);
  event_data.Set("modifiers", modifiers);
  devtools_window_->bindings_->CallClientMethod(
      "DevToolsAPI", "keyEventUnhandled", base::Value(std::move(event_data)));
  return true;
}

int DevToolsEventForwarder::CombineKeyCodeAndModifiers(int key_code,
                                                       int modifiers) {
  return key_code | (modifiers << 16);
}

bool DevToolsEventForwarder::KeyWhitelistingAllowed(int key_code,
                                                    int modifiers) {
  return (ui::VKEY_F1 <= key_code && key_code <= ui::VKEY_F12) ||
      modifiers != 0;
}

void DevToolsWindow::OpenNodeFrontend() {
  DevToolsWindow::OpenNodeFrontendWindow(
      profile_, DevToolsOpenedByAction::kOpenForNodeFromAnotherTarget);
}

// DevToolsWindow::Throttle ------------------------------------------

class DevToolsWindow::Throttle : public content::NavigationThrottle {
 public:
  Throttle(content::NavigationHandle* navigation_handle,
           DevToolsWindow* devtools_window)
      : content::NavigationThrottle(navigation_handle),
        devtools_window_(devtools_window) {
    devtools_window_->throttle_ = this;
  }

  Throttle(const Throttle&) = delete;
  Throttle& operator=(const Throttle&) = delete;

  ~Throttle() override {
    if (devtools_window_)
      devtools_window_->throttle_ = nullptr;
  }

  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return DEFER;
  }

  const char* GetNameForLogging() override { return "DevToolsWindowThrottle"; }

  void ResumeThrottle() {
    if (devtools_window_) {
      devtools_window_->throttle_ = nullptr;
      devtools_window_ = nullptr;
    }
    Resume();
  }

 private:
  raw_ptr<DevToolsWindow> devtools_window_;
};

void DevToolsWindow::MainWebContentsObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_frame,
    content::RenderFrameHost* new_frame) {
  window_->MainWebContentRenderFrameHostChanged(old_frame, new_frame);
}

DevToolsWindow::MainWebContentsObserver::~MainWebContentsObserver() = default;

// Helper class that holds the owned main WebContents for the docked
// devtools window and maintains a keepalive object that keeps the browser
// main loop alive long enough for the WebContents to clean up properly.
class DevToolsWindow::OwnedMainWebContents {
 public:
  explicit OwnedMainWebContents(
      std::unique_ptr<content::WebContents> web_contents)
      : keep_alive_(KeepAliveOrigin::DEVTOOLS_WINDOW,
                    KeepAliveRestartOption::DISABLED),
        web_contents_(std::move(web_contents)) {
    Profile* profile = GetProfileForDevToolsWindow(web_contents_.get());
    DCHECK(profile);
    if (!profile->IsOffTheRecord()) {
      // ScopedProfileKeepAlive does not support OTR profiles.
      profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
          profile, ProfileKeepAliveOrigin::kDevToolsWindow);
    }
  }

  static std::unique_ptr<content::WebContents> TakeWebContents(
      std::unique_ptr<OwnedMainWebContents> instance) {
    return std::move(instance->web_contents_);
  }

 private:
  ScopedKeepAlive keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// DevToolsWindow -------------------------------------------------------------

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

// static
void DevToolsWindow::AddCreationCallbackForTest(
    const CreationCallback& callback) {
  g_creation_callbacks.Get().push_back(callback);
}

// static
void DevToolsWindow::RemoveCreationCallbackForTest(
    const CreationCallback& callback) {
  for (size_t i = 0; i < g_creation_callbacks.Get().size(); ++i) {
    if (g_creation_callbacks.Get().at(i) == callback) {
      g_creation_callbacks.Get().erase(g_creation_callbacks.Get().begin() + i);
      return;
    }
  }
}

DevToolsWindow::~DevToolsWindow() {
  if (throttle_)
    throttle_->ResumeThrottle();

  life_stage_ = kClosing;

  base::RecordAction(base::UserMetricsAction("DevTools_Close"));

  UpdateBrowserWindow();
  UpdateBrowserToolbar();

  if (sharing_infobar_) {
    sharing_infobar_->RemoveSelf();
  }

  capture_handle_.RunAndReset();
  owned_toolbox_web_contents_.reset();

  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  auto it = base::ranges::find(*instances, this);
  CHECK(it != instances->end(), base::NotFatalUntil::M130);
  instances->erase(it);

  if (!close_callback_.is_null())
    std::move(close_callback_).Run();
  // Defer deletion of the main web contents, since we could get here
  // via RenderFrameHostImpl method that expects WebContents to live
  // for some time. See http://crbug.com/997299 for details.
  if (owned_main_web_contents_) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(owned_main_web_contents_));
  }

  // If window gets destroyed during a test run, need to stop the test.
  if (!ready_for_test_callback_.is_null()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(ready_for_test_callback_));
  }
}

// static
void DevToolsWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles);
  registry->RegisterDictionaryPref(prefs::kDevToolsFileSystemPaths);
  registry->RegisterStringPref(prefs::kDevToolsAdbKey, std::string());
  registry->RegisterInt64Pref(prefs::kDevToolsLastOpenTimestamp, 0L);

  registry->RegisterBooleanPref(prefs::kDevToolsDiscoverUsbDevicesEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingEnabled, false);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingDefaultSet,
                                false);
  registry->RegisterDictionaryPref(prefs::kDevToolsPortForwardingConfig);
  registry->RegisterBooleanPref(prefs::kDevToolsDiscoverTCPTargetsEnabled,
                                true);
  registry->RegisterListPref(prefs::kDevToolsTCPDiscoveryConfig);
  registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);
  registry->RegisterBooleanPref(
      prefs::kDevToolsSyncPreferences,
      DevToolsSettings::kSyncDevToolsPreferencesDefault,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsSyncedPreferencesSyncEnabled,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsSyncedPreferencesSyncDisabled);
  registry->RegisterIntegerPref(
      prefs::kDevToolsGenAiSettings,
      static_cast<int>(DevToolsGenAiEnterprisePolicyValue::kAllow));
}

// static
content::WebContents* DevToolsWindow::GetInTabWebContents(
    WebContents* inspected_web_contents,
    DevToolsContentsResizingStrategy* out_strategy) {
  DevToolsWindow* window = GetInstanceForInspectedWebContents(
      inspected_web_contents);
  if (!window || window->life_stage_ == kClosing)
    return nullptr;

  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->life_stage_ == kLoadCompleted ||
      window->life_stage_ == kIsDockedSet;
  if (!is_docked_set)
    return nullptr;

  // Undocked window should have toolbox web contents.
  if (!window->is_docked_ && !window->toolbox_web_contents_)
    return nullptr;

  if (out_strategy)
    out_strategy->CopyFrom(window->contents_resizing_strategy_);

  return window->is_docked_ ? window->main_web_contents_.get()
                            : window->toolbox_web_contents_.get();
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedWebContents(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents || !g_devtools_window_instances.IsCreated())
    return nullptr;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return *it;
  }
  return nullptr;
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::WebContents* web_contents) {
  if (!web_contents || !g_devtools_window_instances.IsCreated())
    return false;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents ||
        (*it)->toolbox_web_contents_ == web_contents)
      return true;
  }
  return false;
}

// static
void DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    const scoped_refptr<DevToolsAgentHost>& worker_agent,
    DevToolsOpenedByAction opened_by) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent.get());
  if (!window) {
    base::RecordAction(base::UserMetricsAction("DevTools_InspectWorker"));
    window = Create(profile, nullptr, kFrontendWorker, std::string(), false, "",
                    "", worker_agent->IsAttached(),
                    /* browser_connection */ true, opened_by);
    if (!window)
      return;
    window->bindings_->AttachViaBrowserTarget(worker_agent);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    DevToolsOpenedByAction opened_by) {
  ToggleDevToolsWindow(inspected_web_contents, nullptr, true,
                       DevToolsToggleAction::Show(), "", opened_by);
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    Profile* profile,
    DevToolsOpenedByAction opened_by) {
  ToggleDevToolsWindow(inspected_web_contents, profile, true,
                       DevToolsToggleAction::Show(), "");
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile,
    DevToolsOpenedByAction opened_by) {
  OpenDevToolsWindow(agent_host, profile, false /* use_bundled_frontend */,
                     opened_by);
}

// static
void DevToolsWindow::OpenDevToolsWindowWithBundledFrontend(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile,
    DevToolsOpenedByAction opened_by) {
  OpenDevToolsWindow(agent_host, profile, true /* use_bundled_frontend */,
                     opened_by);
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    scoped_refptr<content::DevToolsAgentHost> agent_host,
    Profile* profile,
    bool use_bundled_frontend,
    DevToolsOpenedByAction opened_by) {
  if (!profile)
    profile = Profile::FromBrowserContext(agent_host->GetBrowserContext());

  if (!profile)
    return;

  std::string type = agent_host->GetType();

  bool is_worker = type == DevToolsAgentHost::kTypeServiceWorker ||
                   type == DevToolsAgentHost::kTypeSharedWorker ||
                   type == DevToolsAgentHost::kTypeSharedStorageWorklet;

  if (!agent_host->GetFrontendURL().empty()) {
    DevToolsWindow::OpenExternalFrontend(profile, agent_host->GetFrontendURL(),
                                         agent_host, use_bundled_frontend,
                                         opened_by);
    return;
  }

  if (is_worker) {
    DevToolsWindow::OpenDevToolsWindowForWorker(profile, agent_host, opened_by);
    return;
  }

  if (type == content::DevToolsAgentHost::kTypeFrame) {
    DevToolsWindow::OpenDevToolsWindowForFrame(profile, agent_host, opened_by);
    return;
  }

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (web_contents)
    DevToolsWindow::OpenDevToolsWindow(web_contents, profile, opened_by);
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    const DevToolsToggleAction& action,
    DevToolsOpenedByAction opened_by) {
  ToggleDevToolsWindow(inspected_web_contents, nullptr, true, action, "",
                       opened_by);
}

// static
void DevToolsWindow::OpenDevToolsWindowForFrame(
    Profile* profile,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host,
    DevToolsOpenedByAction opened_by) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (!window) {
    window = DevToolsWindow::Create(
        profile, nullptr, kFrontendDefault, std::string(), false, std::string(),
        std::string(), agent_host->IsAttached(), false, opened_by);
    if (!window)
      return;
    window->bindings_->AttachTo(agent_host);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::ToggleDevToolsWindow(Browser* browser,
                                          const DevToolsToggleAction& action,
                                          DevToolsOpenedByAction opened_by) {
  if (action.type() == DevToolsToggleAction::kToggle &&
      browser->is_type_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return;
  }

  ToggleDevToolsWindow(browser->tab_strip_model()->GetActiveWebContents(),
                       nullptr, action.type() == DevToolsToggleAction::kInspect,
                       action, "", opened_by);
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host,
    bool use_bundled_frontend,
    DevToolsOpenedByAction opened_by) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (window) {
    window->ScheduleShow(DevToolsToggleAction::Show());
    return;
  }

  std::string type = agent_host->GetType();
  if (type == "node") {
    // Direct node targets will always open using ToT front-end.
    window = Create(profile, nullptr, kFrontendV8, std::string(), false,
                    std::string(), std::string(), agent_host->IsAttached(),
                    /* browser_connection */ false, opened_by);
  } else {
    bool is_worker = type == DevToolsAgentHost::kTypeServiceWorker ||
                     type == DevToolsAgentHost::kTypeSharedWorker ||
                     type == DevToolsAgentHost::kTypeSharedStorageWorklet;

    FrontendType frontend_type =
        is_worker ? kFrontendRemoteWorker : kFrontendRemote;
    std::string effective_frontend_url =
        use_bundled_frontend ? kFallbackFrontendURL
                             : DevToolsUI::GetProxyURL(frontend_url).spec();
    if (type == "tab") {
      if (effective_frontend_url.find("?") == std::string::npos) {
        effective_frontend_url += "?targetType=tab";
      } else {
        effective_frontend_url += "&targetType=tab";
      }
    }
    window =
        Create(profile, nullptr, frontend_type, effective_frontend_url, false,
               std::string(), std::string(), agent_host->IsAttached(),
               /* browser_connection */ false, opened_by);
  }
  if (!window)
    return;
  window->bindings_->AttachTo(agent_host);
  window->close_on_detach_ = false;
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::OpenNodeFrontendWindow(
    Profile* profile,
    DevToolsOpenedByAction opened_by) {
  for (DevToolsWindow* window : g_devtools_window_instances.Get()) {
    if (window->frontend_type_ == kFrontendNode) {
      window->ActivateWindow();
      return window;
    }
  }

  DevToolsWindow* window = Create(
      profile, nullptr, kFrontendNode, std::string(), false, std::string(),
      std::string(), false, /* browser_connection */ false, opened_by);
  if (!window)
    return nullptr;
  window->bindings_->AttachTo(DevToolsAgentHost::CreateForDiscovery());
  window->ScheduleShow(DevToolsToggleAction::Show());
  return window;
}

// static
Profile* DevToolsWindow::GetProfileForDevToolsWindow(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsPrimaryOTRProfile()) {
    return profile;
  }
  return profile->GetOriginalProfile();
}

namespace {

scoped_refptr<DevToolsAgentHost> GetOrCreateDevToolsHostForWebContents(
    WebContents* wc) {
  return DevToolsAgentHost::GetOrCreateForTab(wc);
}

}  // namespace

// static
void DevToolsWindow::ToggleDevToolsWindow(
    content::WebContents* inspected_web_contents,
    Profile* profile,
    bool force_open,
    const DevToolsToggleAction& action,
    const std::string& settings,
    DevToolsOpenedByAction toggled_by) {
  scoped_refptr<DevToolsAgentHost> agent(
      GetOrCreateDevToolsHostForWebContents(inspected_web_contents));
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    if (!profile)
      profile = GetProfileForDevToolsWindow(inspected_web_contents);
    base::RecordAction(base::UserMetricsAction("DevTools_InspectRenderer"));
    std::string panel;
    switch (action.type()) {
      case DevToolsToggleAction::kInspect:
      case DevToolsToggleAction::kShowElementsPanel:
        panel = "elements";
        break;
      case DevToolsToggleAction::kShowConsolePanel:
        panel = "console";
        break;
      case DevToolsToggleAction::kPauseInDebugger:
        panel = "sources";
        break;
      case DevToolsToggleAction::kShow:
      case DevToolsToggleAction::kToggle:
      case DevToolsToggleAction::kReveal:
      case DevToolsToggleAction::kNoOp:
        break;
    }
    window = Create(profile, inspected_web_contents, kFrontendDefault,
                    std::string(), true, settings, panel, agent->IsAttached(),
                    /* browser_connection */ false, toggled_by);
    if (!window)
      return;
    window->bindings_->AttachTo(agent.get());
    do_open = true;
    if (toggled_by != DevToolsOpenedByAction::kUnknown) {
      LogDevToolsOpenedByAction(toggled_by);
      LogDevToolsOpenedUKM(inspected_web_contents);
    }
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked_ || do_open) {
    window->ScheduleShow(action);
  } else {
    DevToolsClosedByAction closed_by;
    switch (toggled_by) {
      case DevToolsOpenedByAction::kMainMenuOrMainShortcut:
        closed_by = DevToolsClosedByAction::kMainMenuOrMainShortcut;
        break;
      case DevToolsOpenedByAction::kToggleShortcut:
        closed_by = DevToolsClosedByAction::kToggleShortcut;
        break;
      case DevToolsOpenedByAction::kPinnedToolbarButton:
        closed_by = DevToolsClosedByAction::kPinnedToolbarButton;
        break;
      default:
        closed_by = DevToolsClosedByAction::kUnknown;
        break;
    }
    window->Close(closed_by);
  }
}

// static
void DevToolsWindow::InspectElement(
    content::RenderFrameHost* inspected_frame_host,
    int x,
    int y) {
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(inspected_frame_host);
  scoped_refptr<DevToolsAgentHost> agent(
      GetOrCreateDevToolsHostForWebContents(web_contents));
  agent->InspectElement(inspected_frame_host, x, y);
  bool should_measure_time = !FindDevToolsWindow(agent.get());
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenDevToolsWindow(web_contents, DevToolsToggleAction::ShowElementsPanel(),
                     DevToolsOpenedByAction::kContextMenuInspect);
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  if (window && should_measure_time)
    window->inspect_element_start_time_ = start_time;
}

// static
void DevToolsWindow::LogDevToolsOpenedByAction(
    DevToolsOpenedByAction opened_by) {
  base::UmaHistogramEnumeration("DevTools.OpenedByAction", opened_by);
}

// static
void DevToolsWindow::LogDevToolsOpenedUKM(content::WebContents* web_contents) {
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::DevTools_Opened(source_id).SetHasOccurred(true).Record(
      ukm::UkmRecorder::Get());
}

// static
std::unique_ptr<content::NavigationThrottle>
DevToolsWindow::MaybeCreateNavigationThrottle(
    content::NavigationHandle* handle) {
  WebContents* web_contents = handle->GetWebContents();
  if (!web_contents || !web_contents->HasLiveOriginalOpenerChain() ||
      !web_contents->GetController()
           .GetLastCommittedEntry()
           ->IsInitialEntry()) {
    return nullptr;
  }

  WebContents* opener =
      handle->GetWebContents()->GetFirstWebContentsInLiveOriginalOpenerChain();
  DevToolsWindow* window = GetInstanceForInspectedWebContents(opener);
  if (!window || !window->open_new_window_for_popups_ ||
      GetInstanceForInspectedWebContents(web_contents))
    return nullptr;

  DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsOpenedByAction::kAutomaticForNewTarget);
  window = GetInstanceForInspectedWebContents(web_contents);
  if (!window)
    return nullptr;

  return std::make_unique<Throttle>(handle, window);
}

void DevToolsWindow::ScheduleShow(const DevToolsToggleAction& action) {
  if (life_stage_ == kLoadCompleted) {
    Show(action);
    return;
  }

  // Action will be done only after load completed.
  action_on_load_ = action;

  if (!can_dock_) {
    // No harm to show always-undocked window right away.
    is_docked_ = false;
    Show(DevToolsToggleAction::Show());
  }
}

void DevToolsWindow::Show(const DevToolsToggleAction& action) {
  if (life_stage_ == kClosing)
    return;

  if (action.type() == DevToolsToggleAction::kNoOp)
    return;
  if (is_docked_) {
    DCHECK(can_dock_);
    Browser* inspected_browser = nullptr;
    int inspected_tab_index = -1;
    FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                    &inspected_browser,
                                    &inspected_tab_index);
    DCHECK(inspected_browser);
    DCHECK_NE(-1, inspected_tab_index);

    RegisterModalDialogManager(inspected_browser);

    // Tell inspected browser to update splitter and switch to inspected panel.
    BrowserWindow* inspected_window = inspected_browser->window();
    main_web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(
        inspected_tab_index,
        TabStripUserGestureDetails(
            TabStripUserGestureDetails::GestureType::kOther));

    inspected_window->UpdateDevTools();
    main_web_contents_->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    main_web_contents_->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(main_web_contents_);
    OverrideAndSyncDevToolsRendererPrefs();

    MaybeShowSharedProcessInfobar();

    DoAction(action);
    return;
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action.type() != DevToolsToggleAction::kInspect);

  if (!browser_)
    CreateDevToolsBrowser();

  // Ignore action if browser does not exist and could not be created.
  if (!browser_)
    return;

  RegisterModalDialogManager(browser_);
  MaybeShowSharedProcessInfobar();

  if (should_show_window) {
    browser_->window()->Show();
    main_web_contents_->SetInitialFocus();
  }
  if (toolbox_web_contents_)
    UpdateBrowserWindow();

  DoAction(action);
}

// static
bool DevToolsWindow::HandleBeforeUnload(WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  DevToolsWindow* window = AsDevToolsWindow(frontend_contents);
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool DevToolsWindow::InterceptPageBeforeUnload(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->life_stage_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of devtools inspecting another devtools instance by passing
  // the call up to the inspecting devtools instance.
  // TODO(chrisha): Make devtools handle |auto_cancel=false| unload handler
  // dispatches; otherwise, discarding queries can cause unload dialogs to
  // pop-up for tabs with an attached devtools.
  if (!DevToolsWindow::InterceptPageBeforeUnload(window->main_web_contents_)) {
    window->main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  }
  return true;
}

// static
bool DevToolsWindow::NeedsToInterceptBeforeUnload(
    WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  return window && !window->intercepted_page_beforeunload_ &&
         window->life_stage_ == kLoadCompleted;
}

// static
bool DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(
    Browser* browser) {
  DCHECK(browser->is_type_devtools());
  // When FastUnloadController is used, devtools frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->tab_strip_model()->empty())
    return true;
  DevToolsWindow* window = AsDevToolsWindow(browser);
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void DevToolsWindow::OnPageCloseCanceled(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to devtools opened on devtools if any.
  DevToolsWindow::OnPageCloseCanceled(window->main_web_contents_);
}

DevToolsWindow::DevToolsWindow(FrontendType frontend_type,
                               Profile* profile,
                               std::unique_ptr<WebContents> main_web_contents,
                               DevToolsUIBindings* bindings,
                               WebContents* inspected_web_contents,
                               bool can_dock,
                               DevToolsOpenedByAction opened_by)
    : frontend_type_(frontend_type),
      profile_(profile),
      main_web_contents_(main_web_contents.get()),
      main_web_contents_observer_(*main_web_contents_, *this),
      toolbox_web_contents_(nullptr),
      bindings_(bindings),
      browser_(nullptr),
      is_docked_(true),
      owned_main_web_contents_(
          std::make_unique<OwnedMainWebContents>(std::move(main_web_contents))),
      can_dock_(can_dock),
      close_on_detach_(true),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      life_stage_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(DevToolsToggleAction::NoOp()),
      intercepted_page_beforeunload_(false),
      ready_for_test_(false),
      opened_by_(opened_by),
      closed_by_(DevToolsClosedByAction::kUnknown) {
  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |life_stage_ == kLoadCompleted|.
  main_web_contents_->SetDelegate(this);
  // Bindings take ownership over devtools as its delegate.
  bindings_->SetDelegate(this);
  // DevTools uses PageZoom::Zoom(), so main_web_contents_ requires a
  // ZoomController.
  zoom::ZoomController::CreateForWebContents(main_web_contents_);
  zoom::ZoomController::FromWebContents(main_web_contents_)
      ->SetShowsNotificationBubble(false);

  g_devtools_window_instances.Get().push_back(this);

  // There is no inspected_web_contents in case of various workers.
  if (inspected_web_contents) {
    Observe(inspected_web_contents);
  }

  extensions::SetViewType(main_web_contents_,
                          extensions::mojom::ViewType::kDeveloperTools);

  // Initialize docked page to be of the right size.
  if (can_dock_ && inspected_web_contents) {
    content::RenderWidgetHostView* inspected_view =
        inspected_web_contents->GetRenderWidgetHostView();
    if (inspected_view && main_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size = inspected_view->GetViewBounds().size();
      main_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
  }

  event_forwarder_ = std::make_unique<DevToolsEventForwarder>(this);

  // Tag the DevTools main WebContents with its TaskManager specific UserData
  // so that it shows up in the task manager.
  task_manager::WebContentsTags::CreateForDevToolsContents(main_web_contents_);

  std::vector<base::RepeatingCallback<void(DevToolsWindow*)>> copy(
      g_creation_callbacks.Get());
  for (const auto& callback : copy)
    callback.Run(this);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      language::prefs::kAcceptLanguages,
      base::BindRepeating(&DevToolsWindow::OnLocaleChanged,
                          base::Unretained(this)));

  int64_t now_timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMilliseconds();
  profile_->GetPrefs()->SetInt64(prefs::kDevToolsLastOpenTimestamp,
                                 now_timestamp);
}

// static
bool DevToolsWindow::AllowDevToolsFor(Profile* profile,
                                      content::WebContents* web_contents) {
  // Don't allow DevTools UI in kiosk mode, because the DevTools UI would be
  // broken there. See https://crbug.com/514551 for context.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return false;

  return ChromeDevToolsManagerDelegate::AllowInspection(profile, web_contents);
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    content::WebContents* inspected_web_contents,
    FrontendType frontend_type,
    const std::string& frontend_url,
    bool can_dock,
    const std::string& settings,
    const std::string& panel,
    bool has_other_clients,
    bool browser_connection,
    DevToolsOpenedByAction opened_by) {
  if (!AllowDevToolsFor(profile, inspected_web_contents))
    return nullptr;

  if (inspected_web_contents) {
    // Check for a place to dock.
    Browser* browser = nullptr;
    int tab;
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents, &browser,
                                         &tab) ||
        !browser->is_type_normal()) {
      can_dock = false;
    }
  }

  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_type, frontend_url, can_dock, panel,
                          has_other_clients, browser_connection));
  std::unique_ptr<WebContents> main_web_contents =
      WebContents::Create(WebContents::CreateParams(profile));
  main_web_contents->GetController().LoadURL(
      DecorateFrontendURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  DevToolsUIBindings* bindings =
      DevToolsUIBindings::ForWebContents(main_web_contents.get());

  if (!bindings)
    return nullptr;
  if (!settings.empty())
    SetPreferencesFromJson(profile, settings);
  return new DevToolsWindow(frontend_type, profile,
                            std::move(main_web_contents), bindings,
                            inspected_web_contents, can_dock, opened_by);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    FrontendType frontend_type,
                                    const std::string& frontend_url,
                                    bool can_dock,
                                    const std::string& panel,
                                    bool has_other_clients,
                                    bool browser_connection) {
  std::string url;

  std::string remote_base =
      "?remoteBase=" + DevToolsUI::GetRemoteBaseURL().spec();

  const std::string valid_frontend =
      frontend_url.empty() ? chrome::kChromeUIDevToolsURL : frontend_url;

  // remoteFrontend is here for backwards compatibility only.
  std::string remote_frontend =
      valid_frontend + ((valid_frontend.find("?") == std::string::npos)
                            ? "?remoteFrontend=true"
                            : "&remoteFrontend=true");
  switch (frontend_type) {
    case kFrontendDefault:
      url = kDefaultFrontendURL + remote_base + "&targetType=tab";
      if (can_dock)
        url += "&can_dock=true";
      if (!panel.empty())
        url += "&panel=" + panel;
      break;
    case kFrontendWorker:
      url = kWorkerFrontendURL + remote_base;
      break;
    case kFrontendV8:
      url = kJSFrontendURL + remote_base;
      break;
    case kFrontendNode:
      url = kNodeFrontendURL + remote_base;
      break;
    case kFrontendRemote:
      url = remote_frontend;
      break;
    case kFrontendRemoteWorker:
      // isSharedWorker is here for backwards compatibility only.
      url = remote_frontend + "&isSharedWorker=true";
      break;
  }

  if (has_other_clients)
    url += "&hasOtherClients=true";
  if (browser_connection)
    url += "&browserConnection=true";

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnsafelyDisableDevToolsSelfXssWarnings)) {
    url += "&disableSelfXssWarnings=true";
  }

#if BUILDFLAG(CHROME_FOR_TESTING)
  url += "&isChromeForTesting=true";
#endif

  return DevToolsUIBindings::SanitizeFrontendURL(GURL(url));
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  if (!agent_host || !g_devtools_window_instances.IsCreated())
    return nullptr;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->bindings_->IsAttachedTo(agent_host))
      return *it;
  }
  return nullptr;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::WebContents* web_contents) {
  if (!web_contents || !g_devtools_window_instances.IsCreated())
    return nullptr;
  DevToolsWindows* instances = g_devtools_window_instances.Pointer();
  for (auto it(instances->begin()); it != instances->end(); ++it) {
    if ((*it)->main_web_contents_ == web_contents)
      return *it;
  }
  return nullptr;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(Browser* browser) {
  DCHECK(browser->is_type_devtools());
  if (browser->tab_strip_model()->empty())
    return nullptr;
  WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(0);
  return AsDevToolsWindow(contents);
}

WebContents* DevToolsWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  DCHECK(source == main_web_contents_);
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    // TODO(https://crbug.com/40275094): Plumb the `navigation_handle_callback`.
    return OpenURLFromInspectedTab(params);
  }
  // TODO(https://crbug.com/40275094): Plumb the `navigation_handle_callback`.
  main_web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                             false);
  return main_web_contents_;
}

WebContents* DevToolsWindow::OpenURLFromInspectedTab(
    const content::OpenURLParams& params) {
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents)
    return nullptr;
  content::OpenURLParams modified = params;
  modified.referrer = content::Referrer();
  return inspected_web_contents->OpenURL(modified,
                                         /*navigation_handle_callback=*/{});
}

void DevToolsWindow::ActivateContents(WebContents* contents) {
  if (is_docked_) {
    WebContents* inspected_tab = GetInspectedWebContents();
    if (inspected_tab)
      inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else if (browser_) {
    browser_->window()->Activate();
  }
}

WebContents* DevToolsWindow::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (new_contents.get() == toolbox_web_contents_) {
    owned_toolbox_web_contents_ = std::move(new_contents);
    owned_toolbox_web_contents_->SetOwnerLocationForDebug(FROM_HERE);

    toolbox_web_contents_->SetDelegate(
        new DevToolsToolboxDelegate(toolbox_web_contents_, web_contents()));
    if (main_web_contents_->GetRenderWidgetHostView() &&
        toolbox_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size =
          main_web_contents_->GetRenderWidgetHostView()->GetViewBounds().size();
      toolbox_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
    UpdateBrowserWindow();
    return nullptr;
  }

  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, std::move(new_contents), target_url, disposition,
        window_features, user_gesture, was_blocked);
  }
  return nullptr;
}

void DevToolsWindow::WebContentsCreated(WebContents* source_contents,
                                        int opener_render_process_id,
                                        int opener_render_frame_id,
                                        const std::string& frame_name,
                                        const GURL& target_url,
                                        WebContents* new_contents) {
  if (target_url.SchemeIs(content::kChromeDevToolsScheme) &&
      target_url.path().rfind("device_mode_emulation_frame.html") !=
          std::string::npos) {
    CHECK(can_dock_);

    // Ownership will be passed in DevToolsWindow::AddNewContents.
    capture_handle_.RunAndReset();
    if (owned_toolbox_web_contents_)
      owned_toolbox_web_contents_.reset();
    toolbox_web_contents_ = new_contents;

    // Tag the DevTools toolbox WebContents with its TaskManager specific
    // UserData so that it shows up in the task manager.
    task_manager::WebContentsTags::CreateForDevToolsContents(
        toolbox_web_contents_);

    // The toolbox holds a placeholder for the inspected WebContents. When the
    // placeholder is resized, a frame is requested. The inspected WebContents
    // is resized when the frame is rendered. Force rendering of the toolbox at
    // all times, to make sure that a frame can be rendered even when the
    // inspected WebContents fully covers the toolbox. https://crbug.com/828307
    capture_handle_ = toolbox_web_contents_->IncrementCapturerCount(
        gfx::Size(),
        /*stay_hidden=*/false,
        /*stay_awake=*/false, /*is_activity=*/true);
  }
}

void DevToolsWindow::CloseContents(WebContents* source) {
  // We shouldn't get here as long as we're owned by the browser.
  CHECK(!browser_);
  life_stage_ = kClosing;
  UpdateBrowserWindow();
  // In case of docked main_web_contents_, we own it so delete here.
  // Embedding DevTools window will be deleted as a result of
  // DevToolsUIBindings destruction.
  CHECK(owned_main_web_contents_);
  owned_main_web_contents_.reset();
}

void DevToolsWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  zoom::PageZoom::Zoom(main_web_contents_, zoom_in ? content::PAGE_ZOOM_IN
                                                   : content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::BeforeUnloadFired(WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked devtools window closed directly.
    if (proceed)
      bindings_->Detach();
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    WebContents* inspected_web_contents = GetInspectedWebContents();
    if (proceed) {
      inspected_web_contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      bool should_proceed;
      inspected_web_contents->GetDelegate()->BeforeUnloadFired(
          inspected_web_contents, false, &should_proceed);
      DCHECK(!should_proceed);
    }
    *proceed_to_fire_unload = false;
  }
}

content::KeyboardEventProcessingResult DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event);
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.windows_key_code == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return true;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  return inspected_window && inspected_window->HandleKeyboardEvent(event);
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager(
    WebContents* source) {
  return javascript_dialogs::AppModalDialogManager::GetInstance();
}

std::unique_ptr<content::EyeDropper> DevToolsWindow::OpenEyeDropper(
    content::RenderFrameHost* render_frame_host,
    content::EyeDropperListener* listener) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->OpenEyeDropper(render_frame_host, listener);
  return nullptr;
}

void DevToolsWindow::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool DevToolsWindow::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

void DevToolsWindow::ActivateWindow() {
  if (life_stage_ != kLoadCompleted)
    return;
  if (is_docked_ && GetInspectedBrowserWindow())
    main_web_contents_->Focus();
  else if (!is_docked_ && browser_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::CloseWindow() {
  Close(DevToolsClosedByAction::kCloseButton);
}

void DevToolsWindow::Close(DevToolsClosedByAction closed_by) {
  DCHECK(is_docked_);
  life_stage_ = kClosing;
  main_web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  closed_by_ = closed_by;

  if (sharing_infobar_) {
    sharing_infobar_->RemoveSelf();
    checked_sharing_process_id_ = content::ChildProcessHost::kInvalidUniqueID;
  }
}

void DevToolsWindow::Inspect(scoped_refptr<content::DevToolsAgentHost> host) {
  DevToolsWindow::OpenDevToolsWindow(host, profile_,
                                     DevToolsOpenedByAction::kUnknown);
}

void DevToolsWindow::SetInspectedPageBounds(const gfx::Rect& rect) {
  DevToolsContentsResizingStrategy strategy(rect);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  UpdateBrowserWindow();
}

void DevToolsWindow::InspectElementCompleted() {
  if (!inspect_element_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("DevTools.InspectElement",
        base::TimeTicks::Now() - inspect_element_start_time_);
    inspect_element_start_time_ = base::TimeTicks();
  }
}

void DevToolsWindow::SetIsDocked(bool dock_requested) {
  if (life_stage_ == kClosing)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (life_stage_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    life_stage_ = life_stage_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (life_stage_ == kLoadCompleted)
      LoadCompleted();
    return;
  }

  if (dock_requested == was_docked)
    return;

  if (dock_requested && !was_docked && browser_) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    DCHECK(!owned_main_web_contents_);

    // Removing the only WebContents from the tab strip of browser_ will
    // eventually lead to the destruction of browser_ as well, which is why it's
    // okay to just null the raw pointer here.
    browser_ = nullptr;

    // TODO(crbug.com/40773744): WebContents should be removed with a reason
    // other than kInsertedIntoOtherTabStrip, it's not getting reinserted into
    // another tab strip.
    std::unique_ptr<tabs::TabModel> tab_model =
        tab_strip_model->DetachTabAtForInsertion(
            tab_strip_model->GetIndexOfWebContents(main_web_contents_));
    std::unique_ptr<WebContents> web_contents =
        tabs::TabModel::DestroyAndTakeWebContents(std::move(tab_model));
    owned_main_web_contents_ =
        std::make_unique<OwnedMainWebContents>(std::move(web_contents));
  } else if (!dock_requested && was_docked) {
    UpdateBrowserWindow();
  }

  Show(DevToolsToggleAction::Show());
}

int DevToolsWindow::GetDockStateForLogging() {
  const int kUndocked = 0;
  const int kLeft = 1;
  const int kBottom = 2;
  const int kRight = 3;
  if (!is_docked_) {
    return kUndocked;
  }

  gfx::Rect inspected_page_bounds = contents_resizing_strategy_.bounds();
  if (inspected_page_bounds.x() > 0) {
    return kLeft;
  }
  gfx::Rect devtools_bounds =
      main_web_contents_->GetRenderWidgetHostView()->GetViewBounds();
  return inspected_page_bounds.width() == devtools_bounds.width() ? kBottom
                                                                  : kRight;
}

int DevToolsWindow::GetOpenedByForLogging() {
  return static_cast<int>(opened_by_);
}

int DevToolsWindow::GetClosedByForLogging() {
  return static_cast<int>(closed_by_);
}

void DevToolsWindow::OpenInNewTab(const GURL& url) {
  GURL fixed_url = url;
  WebContents* inspected_web_contents = GetInspectedWebContents();
  int child_id = content::ChildProcessHost::kInvalidUniqueID;
  if (inspected_web_contents) {
    content::RenderViewHost* render_view_host =
        inspected_web_contents->GetPrimaryMainFrame()->GetRenderViewHost();
    if (render_view_host)
      child_id = render_view_host->GetProcess()->GetID();
  }
  // Use about:blank instead of an empty GURL. The browser treats an empty GURL
  // as navigating to the home page, which may be privileged (chrome://newtab/).
  if (!content::ChildProcessSecurityPolicy::GetInstance()->CanRequestURL(
          child_id, fixed_url))
    fixed_url = GURL(url::kAboutBlankURL);
  content::OpenURLParams params(fixed_url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  if (!inspected_web_contents ||
      !inspected_web_contents->OpenURL(params,
                                       /*navigation_handle_callback=*/{})) {
    chrome::ScopedTabbedBrowserDisplayer displayer(profile_);
    chrome::AddSelectedTabWithURL(displayer.browser(), fixed_url,
                                  ui::PAGE_TRANSITION_LINK);
  }
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  OpenInNewTab(GURL(url));
}

void DevToolsWindow::OpenSearchResultsInNewTab(const std::string& query) {
  TemplateURLService* url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  DCHECK(url_service);
  GURL url =
      GetDefaultSearchURLForSearchTerms(url_service, base::UTF8ToUTF16(query));
  OpenInNewTab(url);
}

void DevToolsWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

void DevToolsWindow::SetEyeDropperActive(bool active) {
  WebContents* web_contents = GetInspectedWebContents();
  if (!web_contents)
    return;
  if (active) {
    eye_dropper_ = std::make_unique<DevToolsEyeDropper>(
        web_contents,
        base::BindRepeating(&DevToolsWindow::ColorPickedInEyeDropper,
                            base::Unretained(this)));
  } else {
    eye_dropper_.reset();
  }
}

void DevToolsWindow::ColorPickedInEyeDropper(int r, int g, int b, int a) {
  base::Value::Dict color;
  color.Set("r", r);
  color.Set("g", g);
  color.Set("b", b);
  color.Set("a", a);
  bindings_->CallClientMethod("DevToolsAPI", "eyeDropperPickedColor",
                              base::Value(std::move(color)));
}

void DevToolsWindow::InspectedContentsClosing() {
  if (!close_on_detach_)
    return;
  closed_by_ = DevToolsClosedByAction::kTargetDetach;
  intercepted_page_beforeunload_ = false;
  life_stage_ = kClosing;
  main_web_contents_->ClosePage();
}

infobars::ContentInfoBarManager* DevToolsWindow::GetInfoBarManager() {
  return is_docked_ ? infobars::ContentInfoBarManager::FromWebContents(
                          GetInspectedWebContents())
                    : infobars::ContentInfoBarManager::FromWebContents(
                          main_web_contents_);
}

void DevToolsWindow::RenderProcessGone(bool crashed) {
  // Docked DevToolsWindow owns its main_web_contents_ and must delete it.
  // Undocked main_web_contents_ are owned and handled by browser.
  // see crbug.com/369932
  if (is_docked_) {
    CloseContents(main_web_contents_);
  } else if (browser_ && crashed) {
    browser_->window()->Close();
  }
}

void DevToolsWindow::ShowCertificateViewer(const std::string& cert_chain) {
  std::optional<base::Value> value = base::JSONReader::Read(cert_chain);
  CHECK(value && value->is_list());
  std::vector<std::string> decoded;
  for (const auto& item : value->GetList()) {
    CHECK(item.is_string());
    std::string temp;
    CHECK(base::Base64Decode(item.GetString(), &temp));
    decoded.push_back(std::move(temp));
  }

  std::vector<std::string_view> cert_string_piece;
  for (const auto& str : decoded)
    cert_string_piece.push_back(str);
  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromDERCertChain(cert_string_piece);
  CHECK(cert);

  WebContents* inspected_contents =
      is_docked_ ? GetInspectedWebContents() : main_web_contents_.get();
  Browser* browser = nullptr;
  int tab = 0;
  if (!FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return;
  gfx::NativeWindow parent = browser->window()->GetNativeWindow();
  ::ShowCertificateViewer(inspected_contents, parent, cert.get());
}

void DevToolsWindow::OnLoadCompleted() {
  // First seed inspected tab id for extension APIs.
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    sessions::SessionTabHelper* session_tab_helper =
        sessions::SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      bindings_->CallClientMethod(
          "DevToolsAPI", "setInspectedTabId",
          base::Value(session_tab_helper->session_id().id()));
    }
  }

  if (life_stage_ == kClosing)
    return;

  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (life_stage_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    life_stage_ = life_stage_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (life_stage_ == kLoadCompleted)
    LoadCompleted();
}

void DevToolsWindow::ReadyForTest() {
  ready_for_test_ = true;
  if (!ready_for_test_callback_.is_null())
    std::move(ready_for_test_callback_).Run();
}

void DevToolsWindow::ConnectionReady() {
  if (throttle_)
    throttle_->ResumeThrottle();
}

void DevToolsWindow::SetOpenNewWindowForPopups(bool value) {
  open_new_window_for_popups_ = value;
}

void DevToolsWindow::CreateDevToolsBrowser() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetDict(prefs::kAppWindowPlacement).Find(kDevToolsApp)) {
    // Ensure there is always a default size so that
    // BrowserFrame::InitBrowserFrame can retrieve it later.
    ScopedDictPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::Value::Dict& wp_prefs = update.Get();
    base::Value::Dict dev_tools_defaults;
    dev_tools_defaults.Set("left", 100);
    dev_tools_defaults.Set("top", 100);
    dev_tools_defaults.Set("right", 740);
    dev_tools_defaults.Set("bottom", 740);
    dev_tools_defaults.Set("maximized", false);
    dev_tools_defaults.Set("always_on_top", false);
    wp_prefs.Set(kDevToolsApp, std::move(dev_tools_defaults));
  }

  if (Browser::GetCreationStatusForProfile(profile_) !=
      Browser::CreationStatus::kOk) {
    return;
  }
  browser_ =
      Browser::Create(Browser::CreateParams::CreateForDevTools(profile_));
  browser_->tab_strip_model()->AddWebContents(
      OwnedMainWebContents::TakeWebContents(
          std::move(owned_main_web_contents_)),
      -1, ui::PAGE_TRANSITION_AUTO_TOPLEVEL, AddTabTypes::ADD_ACTIVE);
  OverrideAndSyncDevToolsRendererPrefs();
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  Browser* browser = nullptr;
  int tab;
  return FindInspectedBrowserAndTabIndex(GetInspectedWebContents(), &browser,
                                         &tab)
             ? browser->window()
             : nullptr;
}

void DevToolsWindow::DoAction(const DevToolsToggleAction& action) {
  switch (action.type()) {
    case DevToolsToggleAction::kInspect:
      bindings_->CallClientMethod("DevToolsAPI", "enterInspectElementMode");
      break;

    case DevToolsToggleAction::kShowElementsPanel:
    case DevToolsToggleAction::kPauseInDebugger:
    case DevToolsToggleAction::kShowConsolePanel:
    case DevToolsToggleAction::kShow:
    case DevToolsToggleAction::kToggle:
      // Do nothing.
      break;

    case DevToolsToggleAction::kReveal: {
      const DevToolsToggleAction::RevealParams* params =
          action.params();
      CHECK(params);
      bindings_->CallClientMethod(
          "DevToolsAPI", "revealSourceLine", base::Value(params->url),
          base::Value(static_cast<int>(params->line_number)),
          base::Value(static_cast<int>(params->column_number)));
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(nullptr);
}

void DevToolsWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
}

WebContents* DevToolsWindow::GetInspectedWebContents() {
  return web_contents();
}

void DevToolsWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = DevToolsToggleAction::NoOp();
  if (!load_completed_callback_.is_null()) {
    std::move(load_completed_callback_).Run();
  }
}

void DevToolsWindow::SetLoadCompletedCallback(base::OnceClosure closure) {
  if (life_stage_ == kLoadCompleted || life_stage_ == kClosing) {
    if (!closure.is_null())
      std::move(closure).Run();
    return;
  }
  load_completed_callback_ = std::move(closure);
}

bool DevToolsWindow::ForwardKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}

bool DevToolsWindow::ReloadInspectedWebContents(bool bypass_cache) {
  // Only route reload via front-end if the agent is attached.
  WebContents* wc = GetInspectedWebContents();
  if (!wc || wc->GetCrashedStatus() != base::TERMINATION_STATUS_STILL_RUNNING)
    return false;
  bindings_->CallClientMethod("DevToolsAPI", "reloadInspectedPage",
                              base::Value(bypass_cache));
  return true;
}

void DevToolsWindow::RegisterModalDialogManager(Browser* browser) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      main_web_contents_);
  web_modal::WebContentsModalDialogManager::FromWebContents(main_web_contents_)
      ->SetDelegate(browser);
}

void DevToolsWindow::OnLocaleChanged() {
  OverrideAndSyncDevToolsRendererPrefs();
}

void DevToolsWindow::OverrideAndSyncDevToolsRendererPrefs() {
  main_web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;
  main_web_contents_->GetMutableRendererPrefs()->accept_languages =
      g_browser_process->GetApplicationLocale();
  main_web_contents_->SyncRendererPrefs();
}

void DevToolsWindow::MaybeShowSharedProcessInfobar() {
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents) {
    return;
  }

  // Only show the infobar only if the RenderProcessHost id changes.
  int rph_id =
      inspected_web_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
  if (checked_sharing_process_id_ == rph_id) {
    return;
  }
  checked_sharing_process_id_ = rph_id;

  if (!base::FeatureList::IsEnabled(
          ::features::kDevToolsSharedProcessInfobar) ||
      !base::FeatureList::IsEnabled(
          ::features::kProcessPerSiteUpToMainFrameThreshold)) {
    return;
  }

  content::SiteInstance* site_instance =
      inspected_web_contents->GetPrimaryMainFrame()->GetSiteInstance();
  if (site_instance->GetSiteURL().SchemeIs(extensions::kExtensionScheme)) {
    return;
  }

  size_t primary_main_frame_count = 0;
  inspected_web_contents->GetPrimaryMainFrame()
      ->GetProcess()
      ->ForEachRenderFrameHost(
          [&primary_main_frame_count](
              content::RenderFrameHost* render_frame_host) {
            if (render_frame_host->IsInPrimaryMainFrame()) {
              ++primary_main_frame_count;
            }
          });

  // Dismiss old infobar.
  if (sharing_infobar_) {
    sharing_infobar_->RemoveSelf();
  }

  if (primary_main_frame_count > 1) {
    auto* info_bar_manager = GetInfoBarManager();
    sharing_infobar_ = info_bar_manager->AddInfoBar(
        CreateConfirmInfoBar(std::make_unique<ProcessSharingInfobarDelegate>(
            inspected_web_contents)));
    info_bar_manager->AddObserver(this);
  }
}

void DevToolsWindow::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                      bool animate) {
  if (sharing_infobar_ == infobar) {
    infobar->owner()->RemoveObserver(this);
    sharing_infobar_ = nullptr;
  }
}

void DevToolsWindow::PrimaryPageChanged(content::Page& page) {
  MaybeShowSharedProcessInfobar();
}

void DevToolsWindow::MainWebContentRenderFrameHostChanged(
    content::RenderFrameHost* old_frame,
    content::RenderFrameHost* new_frame) {
  DevToolsUIBindings* new_bindings =
      DevToolsUIBindings::ForWebContents(main_web_contents_);
  if (!new_bindings) {
    return;
  }
  bindings_->TransferDelegate(*new_bindings);
  bindings_ = new_bindings;
}
