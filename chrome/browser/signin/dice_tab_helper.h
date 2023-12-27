// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_
#define CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_

#include "base/functional/callback_forward.h"
#include "components/signin/public/base/signin_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
}

struct CoreAccountInfo;
class SigninUIError;

// Tab helper used for DICE to tag signin tabs. Signin tabs can be reused.
class DiceTabHelper : public content::WebContentsUserData<DiceTabHelper>,
                      public content::WebContentsObserver {
 public:
  // Callback starting Sync. This is a repeating callback, because multiple
  // `ProcessDiceHeaderDelegateImpl` may make copies of it.
  using EnableSyncCallback =
      base::RepeatingCallback<void(Profile*,
                                   signin_metrics::AccessPoint,
                                   signin_metrics::PromoAction,
                                   content::WebContents*,
                                   const CoreAccountInfo&)>;

  // Callback displaying a signin error to the user. This is a repeating
  // callback, because multiple `ProcessDiceHeaderDelegateImpl` may make copies
  // of it.
  using ShowSigninErrorCallback = base::RepeatingCallback<
      void(Profile*, content::WebContents*, const SigninUIError&)>;

  // Callback in response to the receiving the signin header.
  using OnSigninHeaderReceived = base::RepeatingCallback<void()>;

  // Returns the default callback to enable sync in a browser window. Does
  // nothing if there is no browser associated with the web contents.
  static EnableSyncCallback GetEnableSyncCallbackForBrowser();

  // Returns the default callback to show a signin error in a browser window.
  // Does nothing if there is no browser associated with the web contents.
  static ShowSigninErrorCallback GetShowSigninErrorCallbackForBrowser();

  DiceTabHelper(const DiceTabHelper&) = delete;
  DiceTabHelper& operator=(const DiceTabHelper&) = delete;

  ~DiceTabHelper() override;

  signin_metrics::AccessPoint signin_access_point() const {
    return state_.signin_access_point;
  }

  signin_metrics::PromoAction signin_promo_action() const {
    return state_.signin_promo_action;
  }

  signin_metrics::Reason signin_reason() const { return state_.signin_reason; }

  const GURL& redirect_url() const { return state_.redirect_url; }

  const GURL& signin_url() const { return state_.signin_url; }

  const EnableSyncCallback& GetEnableSyncCallback() {
    return state_.enable_sync_callback;
  }

  const ShowSigninErrorCallback& GetShowSigninErrorCallback() {
    return state_.show_signin_error_callback;
  }

  const OnSigninHeaderReceived& GetOnSigninHeaderReceived() {
    return state_.on_signin_header_received_callback;
  }

  // Initializes the DiceTabHelper for a new signin flow. Must be called once
  // per signin flow happening in the tab, when the signin URL is being loaded.
  // The `redirect_url` is used after enabling Sync or in case of errors ; it is
  // not used after a successful signin without sync, and in this case the
  // page will navigate to the `continue_url` parameter from `signin_url`.
  void InitializeSigninFlow(
      const GURL& signin_url,
      signin_metrics::AccessPoint access_point,
      signin_metrics::Reason reason,
      signin_metrics::PromoAction promo_action,
      const GURL& redirect_url,
      bool record_signin_started_metrics,
      EnableSyncCallback enable_sync_callback,
      OnSigninHeaderReceived on_signin_header_received_callback,
      ShowSigninErrorCallback show_signin_error_callback);

  // Returns true if this the tab is a re-usable chrome sign-in page (the signin
  // page is loading or loaded in the tab).
  // Returns false if the user or the page has navigated away from |signin_url|.
  bool IsChromeSigninPage() const;

  // Returns true if a signin flow was initialized with the reason
  // kSigninPrimaryAccount and is not yet complete.
  // Note that there is not guarantee that the flow would ever finish, and in
  // some rare cases it is possible that a "non-sync" signin happens while this
  // is true (if the user aborts the flow and then re-uses the same tab for a
  // normal web signin).
  bool IsSyncSigninInProgress() const;

  // Called to notify that the sync signin is complete.
  void OnSyncSigninFlowComplete();

 private:
  friend class content::WebContentsUserData<DiceTabHelper>;
  explicit DiceTabHelper(content::WebContents* web_contents);

  // kStarted: a Sync signin flow was started and not completed.
  // kNotStarted: there is no sync signin flow in progress.
  enum class SyncSigninFlowStatus { kNotStarted, kStarted };

  struct ResetableState {
    ResetableState();
    ~ResetableState();
    ResetableState(const ResetableState& other) = delete;
    ResetableState& operator=(const ResetableState& other) = delete;

    ResetableState(ResetableState&& other);
    ResetableState& operator=(ResetableState&& other);

    GURL redirect_url;
    GURL signin_url;
    EnableSyncCallback enable_sync_callback;
    OnSigninHeaderReceived on_signin_header_received_callback;
    ShowSigninErrorCallback show_signin_error_callback;

    // By default the access point refers to web signin, as after a reset the
    // user may sign in again in the same tab.
    signin_metrics::AccessPoint signin_access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;

    signin_metrics::PromoAction signin_promo_action =
        signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
    signin_metrics::Reason signin_reason =
        signin_metrics::Reason::kUnknownReason;
    SyncSigninFlowStatus sync_signin_flow_status =
        SyncSigninFlowStatus::kNotStarted;
  };

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Returns true if this is a navigation to the signin URL.
  bool IsSigninPageNavigation(
      content::NavigationHandle* navigation_handle) const;

  // Resets the internal state to the initial values.
  void Reset();

  ResetableState state_;

  bool is_chrome_signin_page_ = false;
  bool signin_page_load_recorded_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_
