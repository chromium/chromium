// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/sessions/core/session_types.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class Profile;
class SessionRestoreImpl;

namespace content {
class WebContents;
}

struct StartupTab;
using StartupTabs = std::vector<StartupTab>;

// SessionRestore handles restoring either the last or saved session. Session
// restore come in two variants, asynchronous or synchronous. The synchronous
// variety is meant for startup and blocks until restore is complete.
class SessionRestore {
 public:
  // Bitmask representing behaviors available when restoring a session. Populate
  // using the values below.
  using BehaviorBitmask = uint32_t;

  enum {
    // Indicates the active tab of the supplied browser should be closed.
    CLOBBER_CURRENT_TAB = 1 << 0,

    // Indicates that if there is a problem restoring the last session then a
    // new tabbed browser should be created.
    ALWAYS_CREATE_TABBED_BROWSER = 1 << 1,

    // Restore blocks until complete. This is intended for use during startup
    // when we want to block until restore is complete.
    SYNCHRONOUS = 1 << 2,

    // Restore apps as well.
    RESTORE_APPS = 1 << 3,

    // Restores normal browsers.
    RESTORE_BROWSER = 1 << 4,
  };

  // Notification callback list.
  using CallbackList = base::RepeatingCallbackList<void(Profile*, int)>;
  using RestoredCallback = base::RepeatingCallback<void(Profile*, int)>;

  SessionRestore(const SessionRestore&) = delete;
  SessionRestore& operator=(const SessionRestore&) = delete;

  // Restores the last session. |behavior| is a bitmask of Behaviors, see it
  // for details. If |browser| is non-null the tabs for the first window are
  // added to it. Returns the last active browser.
  //
  // If |startup_tabs| is non-empty, a tab is added for each of the URLs.
  static Browser* RestoreSession(Profile* profile,
                                 Browser* browser,
                                 BehaviorBitmask behavior,
                                 const StartupTabs& startup_tabs);

  // Restores the last session when the last session crashed. It's a wrapper
  // of function RestoreSession.
  static void RestoreSessionAfterCrash(Browser* browser);

  // Opens the startup pages when the last session crashed.
  static void OpenStartupPagesAfterCrash(Browser* browser);

  // Specifically used in the restoration of a foreign session.  This function
  // restores the given session windows to multiple browsers. Returns the
  // created Browsers.
  static std::vector<Browser*> RestoreForeignSessionWindows(
      Profile* profile,
      std::vector<const sessions::SessionWindow*>::const_iterator begin,
      std::vector<const sessions::SessionWindow*>::const_iterator end);

  // Specifically used in the restoration of a foreign session.  This method
  // restores the given session tab to the browser of |source_web_contents| if
  // the disposition is not NEW_WINDOW. Returns the WebContents corresponding
  // to the restored tab. If |disposition| is CURRENT_TAB, |source_web_contents|
  // may be destroyed. If |skip_renderer_creation| is true, depending on if
  // |disposition| is BACKGROUND_TAB, lazily initialize tabs without a renderer.
  static content::WebContents* RestoreForeignSessionTab(
      content::WebContents* source_web_contents,
      const sessions::SessionTab& tab,
      WindowOpenDisposition disposition,
      bool skip_renderer_creation = false);

  // Returns true if we're in the process of restoring |profile|.
  static bool IsRestoring(const Profile* profile);

  // Returns true if synchronously restoring a session.
  static bool IsRestoringSynchronously();

  // Registers a callback that is notified every time session restore completes.
  // Note that 'complete' means all the browsers and tabs have been created but
  // have not necessarily finished loading. The integer supplied to the callback
  // indicates the number of tabs that were created.
  static base::CallbackListSubscription RegisterOnSessionRestoredCallback(
      const RestoredCallback& callback);

  // Add/remove an observer to/from this session restore.
  static void AddObserver(SessionRestoreObserver* observer);
  static void RemoveObserver(SessionRestoreObserver* observer);

  // Get called when the tab loader finishes loading tabs in tab restore even
  // without session restore started.
  static void OnTabLoaderFinishedLoadingTabs();

  // Is called when session restore is going to restore a tab.
  static void OnWillRestoreTab(content::WebContents* web_contents);

  // Is called when windows are read from the last session restore file.
  static void OnGotSession(Profile* profile, bool for_apps, int window_count);

 private:
  friend class SessionRestoreImpl;
  FRIEND_TEST_ALL_PREFIXES(SessionRestoreObserverTest, SingleSessionRestore);
  FRIEND_TEST_ALL_PREFIXES(SessionRestoreObserverTest,
                           SequentialSessionRestores);
  FRIEND_TEST_ALL_PREFIXES(SessionRestoreObserverTest,
                           ConcurrentSessionRestores);
  FRIEND_TEST_ALL_PREFIXES(SessionRestoreObserverTest,
                           TabManagerShouldObserveSessionRestore);

  // Session restore observer list.
  using SessionRestoreObserverList =
      base::ObserverList<SessionRestoreObserver>::Unchecked;

  SessionRestore();

  // Accessor for |*on_session_restored_callbacks_|. Creates a new object the
  // first time so that it always returns a valid object.
  static CallbackList* on_session_restored_callbacks() {
    if (!on_session_restored_callbacks_)
      on_session_restored_callbacks_ = new CallbackList();
    return on_session_restored_callbacks_;
  }

  // Accessor for the observer list. Create the list the first time to always
  // return a valid reference.
  static SessionRestoreObserverList* observers() {
    if (!observers_)
      observers_ = new SessionRestoreObserverList();
    return observers_;
  }

  // Contains all registered callbacks for session restore notifications.
  static CallbackList* on_session_restored_callbacks_;

  // Notify SessionRestoreObservers session restore started. If there are
  // multiple concurrent session restores, observers get notified only once in
  // the first session restore.
  static void NotifySessionRestoreStartedLoadingTabs();

  // Contains all registered observers for session restore events.
  static SessionRestoreObserverList* observers_;

  // Whether session restore started or not.
  static bool session_restore_started_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_H_
