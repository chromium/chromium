// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_

#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class Profile;

namespace base {
class OneShotTimer;
class TickClock;
}  // namespace base

namespace content {
class StoragePartition;
}

namespace extensions {

class WebAuthFlowInfoBarDelegate;

// When enabled, cookies in the `launchWebAuthFlow()` partition are persisted
// across browser restarts.
BASE_DECLARE_FEATURE(kPersistentStorageForWebAuthFlow);

// Controller class for web based auth flows. The WebAuthFlow creates
// a dialog window in the scope approval component app by firing an
// event. A webview embedded in the dialog will navigate to the
// |provider_url| passed to the WebAuthFlow constructor.
//
// The WebAuthFlow monitors the WebContents of the webview, and
// notifies its delegate interface any time the WebContents navigates
// to a new URL or changes title. The delegate is expected to delete
// the flow when navigation reaches a known target location.
//
// The window is not displayed until the first page load
// completes. This allows the flow to complete without flashing a
// window on screen if the provider immediately redirects to the
// target URL.
//
// A WebAuthFlow can be started in Mode::SILENT, which never displays
// a window. If a window would be required, the flow fails.
class WebAuthFlow : public content::WebContentsObserver,
                    public AppWindowRegistry::Observer {
 public:
  enum Mode {
    INTERACTIVE,  // Show UI to the user if necessary.
    SILENT        // No UI should be shown.
  };

  enum Partition {
    GET_AUTH_TOKEN,       // Use the getAuthToken() partition.
    LAUNCH_WEB_AUTH_FLOW  // Use the launchWebAuthFlow() partition.
  };

  enum Failure {
    WINDOW_CLOSED,         // Window closed by user (app or tab).
    INTERACTION_REQUIRED,  // Non-redirect page load in silent mode.
    LOAD_FAILED,
    USER_NAVIGATED_AWAY,  // The user navigated away from the auth page.
    TIMED_OUT
  };

  enum class AbortOnLoad {
    kYes,
    kNo,
  };

  // Maximum time on the total `WebAuthFlow` execution in silent node. This is
  // the default if timeout is not specified.
  static constexpr base::TimeDelta kNonInteractiveMaxTimeout = base::Minutes(1);

  class Delegate {
   public:
    // Called when the auth flow fails. This means that the flow did not result
    // in a successful redirect to a valid redirect URL.
    virtual void OnAuthFlowFailure(Failure failure) = 0;
    // Called on redirects and other navigations to see if the URL should stop
    // the flow.
    virtual void OnAuthFlowURLChange(const GURL& redirect_url) {}
    // Called when the title of the current page changes.
    virtual void OnAuthFlowTitleChange(const std::string& title) {}
    // Called when the web_contents associated with the flow has finished
    // navigation.
    virtual void OnNavigationFinished(
        content::NavigationHandle* navigation_handle) {}

   protected:
    virtual ~Delegate() {}
  };

  // Creates an instance with the given parameters.
  // Caller owns `delegate`.
  WebAuthFlow(Delegate* delegate,
              Profile* profile,
              const GURL& provider_url,
              Mode mode,
              Partition partition,
              AbortOnLoad abort_on_load_for_non_interactive = AbortOnLoad::kYes,
              absl::optional<base::TimeDelta> timeout_for_non_interactive =
                  absl::nullopt);

  WebAuthFlow(const WebAuthFlow&) = delete;
  WebAuthFlow& operator=(const WebAuthFlow&) = delete;

  ~WebAuthFlow() override;

  // Testing clock used to test the effect of load timeout.
  void SetClockForTesting(const base::TickClock* clock,
                          scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Starts the flow.
  virtual void Start();

  // Prevents further calls to the delegate and deletes the flow.
  void DetachDelegateAndDelete();

  // Returns a StoragePartition of the guest webview. Used to inject cookies
  // into Gaia page. Can override for testing.
  virtual content::StoragePartition* GetGuestPartition();

  // Returns an ID string attached to the window. Can override for testing.
  virtual const std::string& GetAppWindowKey() const;

  // Returns the StoragePartitionConfig for a given |partition| used in the
  // WebAuthFlow.
  static content::StoragePartitionConfig GetWebViewPartitionConfig(
      Partition partition,
      content::BrowserContext* browser_context);

  // This call will make the interactive mode, that opens up a browser tab for
  // auth, display an Infobar that shows the extension name.
  void SetShouldShowInfoBar(const std::string& extension_display_name);

  // Returns nullptr if the InfoBar is not displayed.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> GetInfoBarDelegateForTesting();

 private:
  // ::AppWindowRegistry::Observer implementation.
  void OnAppWindowAdded(AppWindow* app_window) override;
  void OnAppWindowRemoved(AppWindow* app_window) override;

  // WebContentsObserver implementation.
  void DidStopLoading() override;
  void InnerWebContentsCreated(
      content::WebContents* inner_web_contents) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void WebContentsDestroyed() override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void BeforeUrlLoaded(const GURL& url);
  void AfterUrlLoaded();

  void MaybeStartTimeout();
  void OnTimeout();

  bool IsObservingProviderWebContents() const;

  bool DisplayAuthPageInPopupWindow();

  void DisplayInfoBar();
  void CloseInfoBar();

  bool IsDisplayingAuthPageInTab() const;

  raw_ptr<Delegate> delegate_ = nullptr;
  const raw_ptr<Profile> profile_;
  const GURL provider_url_;
  const Mode mode_;
  const Partition partition_;

  // Variables used only if displaying the auth flow in an app window.
  raw_ptr<AppWindow> app_window_ = nullptr;
  std::string app_window_key_;
  bool embedded_window_created_ = false;

  // Variables used only if displaying the auth flow in a browser tab.
  //
  // Checks that the auth with browser tab is activated.
  bool using_auth_with_browser_tab_ = false;
  // WebContents used to initialize the authentication. It is not displayed
  // and not owned by browser window. This WebContents is observed by
  // `this`. When this value becomes nullptr, this means that the browser tab
  // has taken ownership and the interactive tab was opened.
  std::unique_ptr<content::WebContents> web_contents_;

  // Internal struct to manage infobar parameters, external calls can only set
  // the extension display name which will force show the info bar through
  // `SetShouldShowInfoBar()`.
  struct InfoBarParameters {
    bool should_show = false;
    std::string extension_display_name;
  };
  InfoBarParameters info_bar_parameters_;

  // WeakPtr to the info bar delegate attached to the auth tab when opened. Used
  // to close the info bar when closing the flow if still valid.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> info_bar_delegate_ = nullptr;

  const AbortOnLoad abort_on_load_for_non_interactive_;
  const absl::optional<base::TimeDelta> timeout_for_non_interactive_;
  std::unique_ptr<base::OneShotTimer> non_interactive_timeout_timer_;
  // Flag indicating that the initial URL was successfully loaded. Influences
  // the error code when the flow times out.
  bool initial_url_loaded_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
