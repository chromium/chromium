// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
#define CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/translate/core/browser/translate_driver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace history {
struct HistoryAddPageArgs;
class HistoryService;
}  // namespace history

class HistoryTabHelper
    : public content::WebContentsObserver,
      public translate::TranslateDriver::LanguageDetectionObserver,
      public content::WebContentsUserData<HistoryTabHelper> {
 public:
  HistoryTabHelper(const HistoryTabHelper&) = delete;
  HistoryTabHelper& operator=(const HistoryTabHelper&) = delete;

  ~HistoryTabHelper() override;

  // Returns the history::HistoryAddPageArgs to use for adding a page to
  // history.
  history::HistoryAddPageArgs CreateHistoryAddPageArgs(
      const GURL& virtual_url,
      base::Time timestamp,
      int nav_entry_id,
      content::NavigationHandle* navigation_handle);

  // Called by password manager code when the PasswordState in this tab was
  // updated.
  void OnPasswordStateUpdated(
      sessions::SerializedNavigationEntry::PasswordState password_state);

  // Fakes that the WebContents is a tab for testing purposes.
  void SetForceEligibleTabForTesting(bool force) {
    force_eligible_tab_for_testing_ = force;
  }

#if BUILDFLAG(IS_ANDROID)
  // Sets App ID that that goes into visit database.
  void SetAppId(const std::string& app_id) { app_id_ = app_id; }
#endif

 private:
  explicit HistoryTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<HistoryTabHelper>;
  FRIEND_TEST_ALL_PREFIXES(HistoryTabHelperTest,
                           CreateAddPageArgsHasOpenerWebContentsFirstPage);
  FRIEND_TEST_ALL_PREFIXES(HistoryTabHelperTest,
                           CreateAddPageArgsHasOpenerWebContentseNotFirstPage);
  FRIEND_TEST_ALL_PREFIXES(HistoryFencedFrameBrowserTest,
                           FencedFrameDoesNotAffectLoadingState);

  // Updates history with the specified navigation. This is called by
  // DidFinishNavigation to update history state.
  void UpdateHistoryForNavigation(
      const history::HistoryAddPageArgs& add_page_args);

  // content::WebContentsObserver implementation.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;
  void TitleWasSet(content::NavigationEntry* entry) override;
  void WebContentsDestroyed() override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;

  // TranslateDriver::LanguageDetectionObserver implementation.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;

  // Helper function to return the history service.  May return null.
  history::HistoryService* GetHistoryService();

  // Returns true if our observed web contents is an eligible tab.
  bool IsEligibleTab(const history::HistoryAddPageArgs& add_page_args) const;

  // Observes LanguageDetectionObserver, which notifies us when the language of
  // the contents of the current page has been determined.
  base::ScopedObservation<translate::TranslateDriver,
                          translate::TranslateDriver::LanguageDetectionObserver>
      translate_observation_{this};

  // True after navigation to a page is complete and the page is currently
  // loading. Only applies to the main frame of the page.
  bool is_loading_ = false;

  // Number of title changes since the loading of the navigation started.
  int num_title_changes_ = 0;

  // The time that the current page finished loading. Only title changes within
  // a certain time period after the page load is complete will be saved to the
  // history system. Only applies to the main frame of the page.
  base::TimeTicks last_load_completion_;

  // Some cached state about the current navigation, used to identify it again
  // once a new navigation has happened.
  struct NavigationState {
    int nav_entry_id;
    GURL url;
  };
  std::optional<NavigationState> cached_navigation_state_;

  // The package name of an app that opens a Custom Tab and visits a URL.
  std::optional<std::string> app_id_ = std::nullopt;

  // Set to true in unit tests to avoid need for a Browser instance.
  bool force_eligible_tab_for_testing_ = false;

  // The `WebContents` that opened the `WebContents` associated with `this` via
  // "Open in New Tab", "Open in New Window", window.open(), etc.
  base::WeakPtr<content::WebContents> opener_web_contents_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_TAB_HELPER_H_
