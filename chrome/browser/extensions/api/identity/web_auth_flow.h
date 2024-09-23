// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class Profile;

namespace base {
class OneShotTimer;
class TickClock;
}  // namespace base

namespace extensions {

class WebAuthFlowInfoBarDelegate;

// Controller class for web based auth flows. The WebAuthFlow creates
// a browser popup window (or a new tab based on the feature setting)
// with a webview that will navigate to the |provider_url| passed to the
// WebAuthFlow constructor.
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
class WebAuthFlow : public content::WebContentsObserver {
 public:
  enum Mode {
    INTERACTIVE,  // Show UI to the user if necessary.
    SILENT        // No UI should be shown.
  };

  enum Failure {
    WINDOW_CLOSED,         // Window closed by user (app or tab).
    INTERACTION_REQUIRED,  // Non-redirect page load in silent mode.
    LOAD_FAILED,
    TIMED_OUT,
    CANNOT_CREATE_WINDOW,  // Couldn't create a browser window.
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
  WebAuthFlow(
      Delegate* delegate,
      Profile* profile,
      const GURL& provider_url,
      Mode mode,
      bool user_gesture,
      AbortOnLoad abort_on_load_for_non_interactive = AbortOnLoad::kYes,
      std::optional<base::TimeDelta> timeout_for_non_interactive = std::nullopt,
      std::optional<gfx::Rect> popup_bounds = std::nullopt);

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

  // Immediately closes the webview and prevents further delegate calls. Can be
  // called before `DetachDelegateAndDelete()` to release resources immediately.
  void Stop();

  // This call will make the interactive mode, that opens up a browser tab for
  // auth, display an Infobar that shows the extension name.
  void SetShouldShowInfoBar(const std::string& extension_display_name);

  // Returns nullptr if the InfoBar is not displayed.
  base::WeakPtr<WebAuthFlowInfoBarDelegate> GetInfoBarDelegateForTesting();

 private:
  // WebContentsObserver implementation.
  void DidStopLoading() override;
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

  bool DisplayAuthPageInPopupWindow();

  void DisplayInfoBar();
  void CloseInfoBar();

  raw_ptr<Delegate> delegate_ = nullptr;
  raw_ptr<Profile> profile_;
  const GURL provider_url_;
  const Mode mode_;
  const bool user_gesture_;

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
  const std::optional<base::TimeDelta> timeout_for_non_interactive_;
  std::unique_ptr<base::OneShotTimer> non_interactive_timeout_timer_;
  const std::optional<gfx::Rect> popup_bounds_;
  // Flag indicating that the initial URL was successfully loaded. Influences
  // the error code when the flow times out.
  bool initial_url_loaded_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDENTITY_WEB_AUTH_FLOW_H_
