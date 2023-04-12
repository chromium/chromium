// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class SessionDataDeleter;

namespace user_prefs {
class PrefRegistrySyncable;
}

// SessionDataService is responsible for deleting SessionOnly cookies and
// site data when the browser or all windows of a profile are closed.
class SessionDataService : public BrowserListObserver, public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Status {
    kUninitialized = 0,
    kInitialized = 1,
    kDeletionStarted = 2,
    kDeletionFinished = 3,
    kNoDeletionDueToForceKeepSessionData = 4,
    kNoDeletionDueToShutdown = 5,
    kMaxValue = kNoDeletionDueToShutdown,
  };

  explicit SessionDataService(Profile* profile,
                              std::unique_ptr<SessionDataDeleter> deleter);
  SessionDataService(const SessionDataService&) = delete;
  SessionDataService& operator=(const SessionDataService&) = delete;
  ~SessionDataService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Starts a deletion of session only cookies and storage unless the deletion
  // is already running or the browser is already shutting down. Called at
  // shutdown.
  void StartCleanup();
  // Instructs this class not to delete session state on shutdown.
  void SetForceKeepSessionState();

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // Starts a deletion of session only cookies and storage unless the deletion
  // is already running or the browser is already shutting down.
  // If |skip_session_cookies| is set to true, only cookies with session-only
  // content setting are removed.
  void StartCleanupInternal(bool skip_session_cookies);

  // Starts another data deletion if the deletion at the end of the last session
  // did not finish.
  void MaybeContinueDeletionFromLastSesssion(Status last_status);

  void SetStatusPref(Status status);
  void OnCleanupAtStartupFinished();
  void OnCleanupAtSessionEndFinished();

  raw_ptr<Profile> profile_;
  std::unique_ptr<SessionDataDeleter> deleter_;
  // A flag that is set to skip session data deletion on restarts.
  bool force_keep_session_state_ = false;
  // A flag to indicate that a deletion was started and further requests for
  // cleanup should be ignored.
  bool cleanup_started_ = false;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_
