// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class BrowserWindow;
class DevToolsWindowTesting;
class DevToolsEventForwarder;
class DevToolsEyeDropper;

namespace content {
class DevToolsAgentHost;
struct NativeWebKeyboardEvent;
class NavigationHandle;
class NavigationThrottle;
class RenderFrameHost;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Values that represent different actions to open and close DevTools window.
// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class DevToolsOpenedByAction {
  kUnknown = 0,
  // Main menu -> More Tools -> Developer Tools
  // or Ctrl+Shift+I shortcut
  kMainMenuOrMainShortcut = 1,
  // Ctrl+Shift+J shortcut to jump to Console
  kConsoleShortcut = 2,
  // Context menu -> Inspect
  kContextMenuInspect = 3,
  // Ctrl+Shift+C shortcut to turn on inspect mode
  kInspectorModeShortcut = 4,
  // Toggle-open via F12
  kToggleShortcut = 5,
  // Link on chrome://inspect
  kInspectLink = 6,
  // Via --auto-open-devtools-for-tabs or "Auto-open DevTools for popups"
  kAutomaticForNewTarget = 7,
  // Re-open when some targets (like apps) reload
  kTargetReload = 8,
  // Open Node DevTools button in a regular app
  kOpenForNodeFromAnotherTarget = 9,
  // User-pinned button in the toolbar
  kPinnedToolbarButton = 10,
  // Add values above this line with a corresponding label in
  // tools/metrics/histograms/metadata/dev/enums.xml
  kMaxValue = kPinnedToolbarButton,
};

enum class DevToolsClosedByAction {
  kUnknown = 0,
  // Main menu -> More Tools -> Developer Tools
  // or Ctrl+Shift+I shortcut
  kMainMenuOrMainShortcut = 1,
  // Toggle-open via F12
  kToggleShortcut = 2,
  kCloseButton = 3,
  kTargetDetach = 4,
  kPinnedToolbarButton = 5,
  kMaxValue = kPinnedToolbarButton,
};

class DevToolsWindow : public DevToolsUIBindings::Delegate,
                       public content::WebContentsDelegate,
                       public content::WebContentsObserver,
                       public infobars::InfoBarManager::Observer {
 public:
  static const char kDevToolsApp[];

  DevToolsWindow(const DevToolsWindow&) = delete;
  DevToolsWindow& operator=(const DevToolsWindow&) = delete;

  ~DevToolsWindow() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns whether DevTools are allowed for the specified
  // |profile| and |web_contents|. If |web_contents| is null,
  // only checks for |profile| in general.
  static bool AllowDevToolsFor(Profile* profile,
                               content::WebContents* web_contents);

  // Return the DevToolsWindow for the given WebContents if one exists,
  // otherwise nullptr.
  static DevToolsWindow* GetInstanceForInspectedWebContents(
      content::WebContents* inspected_web_contents);

  // Return the docked DevTools WebContents for the given inspected WebContents
  // if one exists and should be shown in browser window, otherwise nullptr.
  // This method will return only fully initialized window ready to be
  // presented in UI.
  // If |out_strategy| is not nullptr, it will contain resizing strategy.
  // For immediately-ready-to-use but maybe not yet fully initialized DevTools
  // use |GetInstanceForInspectedRenderViewHost| instead.
  static content::WebContents* GetInTabWebContents(
      content::WebContents* inspected_tab,
      DevToolsContentsResizingStrategy* out_strategy);

  static bool IsDevToolsWindow(content::WebContents* web_contents);
  static DevToolsWindow* AsDevToolsWindow(content::WebContents* web_contents);
  static DevToolsWindow* AsDevToolsWindow(Browser* browser);
  static DevToolsWindow* FindDevToolsWindow(content::DevToolsAgentHost*);

  // Open or reveal DevTools window, and perform the specified action.
  // How to get pointer to the created window see comments for
  // ToggleDevToolsWindow().
  static void OpenDevToolsWindow(content::WebContents* inspected_web_contents,
                                 const DevToolsToggleAction& action,
                                 DevToolsOpenedByAction opened_by);

  // Open or reveal DevTools window, with no special action.
  // How to get pointer to the created window see comments for
  // ToggleDevToolsWindow().
  static void OpenDevToolsWindow(content::WebContents* inspected_web_contents,
                                 DevToolsOpenedByAction opened_by);
  static void OpenDevToolsWindow(content::WebContents* inspected_web_contents,
                                 Profile* profile,
                                 DevToolsOpenedByAction opened_by);

  // Open or reveal DevTools window, with no special action. Use |profile| to
  // open client window in, default to |host|'s profile if none given.
  static void OpenDevToolsWindow(scoped_refptr<content::DevToolsAgentHost> host,
                                 Profile* profile,
                                 DevToolsOpenedByAction opened_by);
  // Similar to previous one, but forces the bundled frontend to be used.
  static void OpenDevToolsWindowWithBundledFrontend(
      scoped_refptr<content::DevToolsAgentHost> host,
      Profile* profile,
      DevToolsOpenedByAction opened_by);

  // Perform specified action for current WebContents inside a |browser|.
  // This may close currently open DevTools window.
  // If DeveloperToolsAvailability policy disallows developer tools for the
  // current WebContents, no DevTools window created. In case if needed pointer
  // to the created window one should use DevToolsAgentHost and
  // DevToolsWindow::FindDevToolsWindow(). E.g.:
  //
  // scoped_refptr<content::DevToolsAgentHost> agent(
  //   content::DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  // DevToolsWindow::ToggleDevToolsWindow(
  //   inspected_web_contents, DevToolsToggleAction::Show());
  // DevToolsWindow* window = DevToolsWindow::FindDevToolsWindow(agent.get());
  //
  static void ToggleDevToolsWindow(
      Browser* browser,
      const DevToolsToggleAction& action,
      DevToolsOpenedByAction opened_by = DevToolsOpenedByAction::kUnknown);

  // Node frontend is always undocked.
  static DevToolsWindow* OpenNodeFrontendWindow(
      Profile* profile,
      DevToolsOpenedByAction opened_by);

  static void InspectElement(content::RenderFrameHost* inspected_frame_host,
                             int x,
                             int y);

  static void LogDevToolsOpenedByAction(DevToolsOpenedByAction opened_by);

  // Logs UKM event when DevTools is opened.
  static void LogDevToolsOpenedUKM(content::WebContents* web_contents);

  static std::unique_ptr<content::NavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* handle);

  // Sets closure to be called after load is done. If already loaded, calls
  // closure immediately.
  void SetLoadCompletedCallback(base::OnceClosure closure);

  // Forwards an unhandled keyboard event to the DevTools frontend.
  bool ForwardKeyboardEvent(const input::NativeWebKeyboardEvent& event);

  // Reloads inspected web contents as if it was triggered from DevTools.
  // Returns true if it has successfully handled reload, false if the caller
  // is to proceed reload without DevTools interception.
  bool ReloadInspectedWebContents(bool bypass_cache);

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

  content::WebContents* OpenURLFromInspectedTab(
      const content::OpenURLParams& params);

  // BeforeUnload interception ////////////////////////////////////////////////

  // In order to preserve any edits the user may have made in devtools, the
  // beforeunload event of the inspected page is hooked - devtools gets the
  // first shot at handling beforeunload and presents a dialog to the user. If
  // the user accepts the dialog then the script is given a chance to handle
  // it. This way 2 dialogs may be displayed: one from the devtools asking the
  // user to confirm that they're ok with their devtools edits going away and
  // another from the webpage as the result of its beforeunload handler.
  // The following set of methods handle beforeunload event flow through
  // devtools window. When the |contents| with devtools opened on them are
  // getting closed, the following sequence of calls takes place:
  // 1. |DevToolsWindow::InterceptPageBeforeUnload| is called and indicates
  //    whether devtools intercept the beforeunload event.
  //    If InterceptPageBeforeUnload() returns true then the following steps
  //    will take place; otherwise only step 4 will be reached and none of the
  //    corresponding functions in steps 2 & 3 will get called.
  // 2. |DevToolsWindow::InterceptPageBeforeUnload| fires beforeunload event
  //    for devtools frontend, which will asynchronously call
  //    |WebContentsDelegate::BeforeUnloadFired| method.
  //    In case of docked devtools window, devtools are set as a delegate for
  //    its frontend, so method |DevToolsWindow::BeforeUnloadFired| will be
  //    called directly.
  //    If devtools window is undocked it's not set as the delegate so the call
  //    to BeforeUnloadFired is proxied through HandleBeforeUnload() rather
  //    than getting called directly.
  // 3a. If |DevToolsWindow::BeforeUnloadFired| is called with |proceed|=false
  //     it calls through to the content's BeforeUnloadFired(), which from the
  //     WebContents perspective looks the same as the |content|'s own
  //     beforeunload dialog having had it's 'stay on this page' button clicked.
  // 3b. If |proceed| = true, then it fires beforeunload event on |contents|
  //     and everything proceeds as it normally would without the Devtools
  //     interception.
  // 4. If the user cancels the dialog put up by either the WebContents or
  //    devtools frontend, then |contents|'s |BeforeUnloadFired| callback is
  //    called with the proceed argument set to false, this causes
  //    |DevToolsWindow::OnPageCloseCancelled| to be called.

  // Devtools window in undocked state is not set as a delegate of
  // its frontend. Instead, an instance of browser is set as the delegate, and
  // thus beforeunload event callback from devtools frontend is not delivered
  // to the instance of devtools window, which is solely responsible for
  // managing custom beforeunload event flow.
  // This is a helper method to route callback from
  // |Browser::BeforeUnloadFired| back to |DevToolsWindow::BeforeUnloadFired|.
  // * |proceed| - true if the user clicked 'ok' in the beforeunload dialog,
  //   false otherwise.
  // * |proceed_to_fire_unload| - output parameter, whether we should continue
  //   to fire the unload event or stop things here.
  // Returns true if devtools window is in a state of intercepting beforeunload
  // event and if it will manage unload process on its own.
  static bool HandleBeforeUnload(content::WebContents* contents,
                                 bool proceed,
                                 bool* proceed_to_fire_unload);

  // Returns true if this contents beforeunload event was intercepted by
  // devtools and false otherwise. If the event was intercepted, caller should
  // not fire beforeunload event on |contents| itself as devtools window will
  // take care of it, otherwise caller should continue handling the event as
  // usual.
  static bool InterceptPageBeforeUnload(content::WebContents* contents);

  // Returns true if devtools browser has already fired its beforeunload event
  // as a result of beforeunload event interception.
  static bool HasFiredBeforeUnloadEventForDevToolsBrowser(Browser* browser);

  // Returns true if devtools window would like to hook beforeunload event
  // of this |contents|.
  static bool NeedsToInterceptBeforeUnload(content::WebContents* contents);

  // Notify devtools window that closing of |contents| was cancelled
  // by user.
  static void OnPageCloseCanceled(content::WebContents* contents);

  content::WebContents* GetInspectedWebContents();

  // content::DevToolsUIBindings::Delegate overrides
  void ActivateWindow() override;

  void MainWebContentRenderFrameHostChanged(
      content::RenderFrameHost* old_frame,
      content::RenderFrameHost* new_frame);

 private:
  friend class DevToolsWindowTesting;
  friend class DevToolsWindowCreationObserver;
  friend class HatsNextWebDialogBrowserTest;

  using CreationCallback = base::RepeatingCallback<void(DevToolsWindow*)>;
  static void AddCreationCallbackForTest(const CreationCallback& callback);
  static void RemoveCreationCallbackForTest(const CreationCallback& callback);

  static void OpenDevToolsWindowForFrame(
      Profile* profile,
      const scoped_refptr<content::DevToolsAgentHost>& agent_host,
      DevToolsOpenedByAction opened_by);
  static void OpenDevToolsWindowForWorker(
      Profile* profile,
      const scoped_refptr<content::DevToolsAgentHost>& worker_agent,
      DevToolsOpenedByAction opened_by);

  // DevTools lifecycle typically follows this way:
  // - Toggle/Open: client call;
  // - Create;
  // - ScheduleShow: setup window to be functional, but not yet show;
  // - DocumentOnLoadCompletedInPrimaryMainFrame: frontend loaded;
  // - SetIsDocked: frontend decided on docking state;
  // - OnLoadCompleted: ready to present frontend;
  // - Show: actually placing frontend WebContents to a Browser or docked place;
  // - DoAction: perform action passed in Toggle/Open;
  // - ...;
  // - CloseWindow: initiates before unload handling;
  // - CloseContents: destroys frontend;
  // - DevToolsWindow is dead once it's main_web_contents dies.
  enum LifeStage {
    kNotLoaded,
    kOnLoadFired, // Implies SetIsDocked was not yet called.
    kIsDockedSet, // Implies DocumentOnLoadCompleted was not yet called.
    kLoadCompleted,
    kClosing
  };

  enum FrontendType {
    kFrontendDefault,
    kFrontendWorker,
    kFrontendV8,
    kFrontendNode,
    kFrontendRemote,
    kFrontendRemoteWorker,
  };

  DevToolsWindow(FrontendType frontend_type,
                 Profile* profile,
                 std::unique_ptr<content::WebContents> main_web_contents,
                 DevToolsUIBindings* bindings,
                 content::WebContents* inspected_web_contents,
                 bool can_dock,
                 DevToolsOpenedByAction opened_by);

  // External frontend is always undocked.
  static void OpenExternalFrontend(
      Profile* profile,
      const std::string& frontend_uri,
      const scoped_refptr<content::DevToolsAgentHost>& agent_host,
      bool use_bundled_frontend,
      DevToolsOpenedByAction opened_by);

  static void OpenDevToolsWindow(scoped_refptr<content::DevToolsAgentHost> host,
                                 Profile* profile,
                                 bool use_bundled_frontend,
                                 DevToolsOpenedByAction opened_by);

  static DevToolsWindow* Create(Profile* profile,
                                content::WebContents* inspected_web_contents,
                                FrontendType frontend_type,
                                const std::string& frontend_url,
                                bool can_dock,
                                const std::string& settings,
                                const std::string& panel,
                                bool has_other_clients,
                                bool browser_connection,
                                DevToolsOpenedByAction opened_by);
  static GURL GetDevToolsURL(Profile* profile,
                             FrontendType frontend_type,
                             const std::string& frontend_url,
                             bool can_dock,
                             const std::string& panel,
                             bool has_other_clients,
                             bool browser_connection);

  static void ToggleDevToolsWindow(
      content::WebContents* web_contents,
      Profile* profile,
      bool force_open,
      const DevToolsToggleAction& action,
      const std::string& settings,
      DevToolsOpenedByAction opened_by = DevToolsOpenedByAction::kUnknown);
  static Profile* GetProfileForDevToolsWindow(
      content::WebContents* web_contents);

  // content::WebContentsDelegate:
  void ActivateContents(content::WebContents* contents) override;
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
  void CloseContents(content::WebContents* source) override;
  void ContentsZoomChange(bool zoom_in) override;
  void BeforeUnloadFired(content::WebContents* tab,
                         bool proceed,
                         bool* proceed_to_fire_unload) override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* source) override;
  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* render_frame_host,
      content::EyeDropperListener* listener) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;
  void Close(DevToolsClosedByAction closed_by);

  // content::DevToolsUIBindings::Delegate overrides
  void CloseWindow() override;
  void Inspect(scoped_refptr<content::DevToolsAgentHost> host) override;
  void SetInspectedPageBounds(const gfx::Rect& rect) override;
  void InspectElementCompleted() override;
  void SetIsDocked(bool is_docked) override;
  void OpenInNewTab(const std::string& url) override;
  void OpenSearchResultsInNewTab(const std::string& url) override;
  void SetWhitelistedShortcuts(const std::string& message) override;
  void SetEyeDropperActive(bool active) override;
  void OpenNodeFrontend() override;
  void InspectedContentsClosing() override;
  void OnLoadCompleted() override;
  void ReadyForTest() override;
  void ConnectionReady() override;
  void SetOpenNewWindowForPopups(bool value) override;
  infobars::ContentInfoBarManager* GetInfoBarManager() override;
  void RenderProcessGone(bool crashed) override;
  void ShowCertificateViewer(const std::string& cert_viewer) override;
  int GetDockStateForLogging() override;
  int GetOpenedByForLogging() override;
  int GetClosedByForLogging() override;

  void OpenInNewTab(const GURL& url);
  void ColorPickedInEyeDropper(int r, int g, int b, int a);

  // content::WebContentsObserver
  using content::WebContentsObserver::BeforeUnloadFired;
  void PrimaryPageChanged(content::Page& page) override;

  // infobars::InfoBarManager::Observer
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // This method creates a new Browser object (if possible), and passes
  // ownership of owned_main_web_contents_ to the tab strip of the Browser.
  void CreateDevToolsBrowser();
  BrowserWindow* GetInspectedBrowserWindow();
  void ScheduleShow(const DevToolsToggleAction& action);
  void Show(const DevToolsToggleAction& action);
  void DoAction(const DevToolsToggleAction& action);
  void LoadCompleted();
  void UpdateBrowserToolbar();
  void UpdateBrowserWindow();

  // Registers a WebContentsModalDialogManager for our WebContents in order to
  // display web modal dialogs triggered by it.
  void RegisterModalDialogManager(Browser* browser);

  // Called when the accepted language changes. |navigator.language| of the
  // DevTools window should match the application language. When the user
  // changes the accepted language then this listener flips the language back
  // to the application language for the DevTools renderer process.
  // Please note that |navigator.language| will have the wrong language for
  // a very short period of time (until this handler has reset it again).
  void OnLocaleChanged();
  void OverrideAndSyncDevToolsRendererPrefs();

  void MaybeShowSharedProcessInfobar();

  FrontendType frontend_type_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> main_web_contents_;

  class MainWebContentsObserver : public content::WebContentsObserver {
   public:
    MainWebContentsObserver(content::WebContents& web_contents,
                            DevToolsWindow& window)
        : WebContentsObserver(&web_contents), window_(window) {}
    ~MainWebContentsObserver() override;

   private:
    void RenderFrameHostChanged(content::RenderFrameHost* old_frame,
                                content::RenderFrameHost* new_frame) override;

    raw_ref<DevToolsWindow> window_;
  };
  MainWebContentsObserver main_web_contents_observer_;

  // DevToolsWindow is informed of the creation of the |toolbox_web_contents_|
  // in WebContentsCreated right before ownership is passed to to DevToolsWindow
  // in AddNewContents(). The former call has information not available in the
  // latter, so it's easiest to record a raw pointer first in
  // |toolbox_web_contents_|, and then update ownership immediately afterwards.
  // TODO(erikchen): If we updated AddNewContents() to also pass back the
  // target url, then we wouldn't need to listen to WebContentsCreated at all.
  raw_ptr<content::WebContents, DanglingUntriaged> toolbox_web_contents_;
  std::unique_ptr<content::WebContents> owned_toolbox_web_contents_;

  raw_ptr<DevToolsUIBindings> bindings_;
  raw_ptr<Browser> browser_;

  // When DevToolsWindow is docked, it owns main_web_contents_. When it isn't
  // docked, the tab strip model owns the main_web_contents_.
  bool is_docked_;
  class OwnedMainWebContents;
  std::unique_ptr<OwnedMainWebContents> owned_main_web_contents_;

  const bool can_dock_;
  bool close_on_detach_;
  LifeStage life_stage_;
  DevToolsToggleAction action_on_load_;
  DevToolsContentsResizingStrategy contents_resizing_strategy_;
  // True if we're in the process of handling a beforeunload event originating
  // from the inspected webcontents, see InterceptPageBeforeUnload for details.
  bool intercepted_page_beforeunload_;
  base::OnceClosure load_completed_callback_;
  base::OnceClosure close_callback_;
  bool ready_for_test_;
  base::OnceClosure ready_for_test_callback_;

  base::TimeTicks inspect_element_start_time_;
  std::unique_ptr<DevToolsEventForwarder> event_forwarder_;
  std::unique_ptr<DevToolsEyeDropper> eye_dropper_;

  const DevToolsOpenedByAction opened_by_;
  DevToolsClosedByAction closed_by_;
  const base::UnguessableToken session_id_for_logging_;

  class Throttle;
  raw_ptr<Throttle> throttle_ = nullptr;
  bool open_new_window_for_popups_ = false;
  raw_ptr<infobars::InfoBar> sharing_infobar_ = nullptr;
  int checked_sharing_process_id_ = content::ChildProcessHost::kInvalidUniqueID;

  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedClosureRunner capture_handle_;

  friend class DevToolsEventForwarder;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_WINDOW_H_
