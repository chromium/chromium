// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_

#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

// SessionDataService is responsible for deleting SessionOnly cookies and
// site data when the browser or all windows of a profile are closed.
class SessionDataService : public BrowserListObserver, public KeyedService {
 public:
  explicit SessionDataService(Profile* profile);
  SessionDataService(const SessionDataService&) = delete;
  SessionDataService& operator=(const SessionDataService&) = delete;
  ~SessionDataService() override;

  // Starts a deletion of session only cookies and storage unless the deletion
  // is already running or the browser is already shutting down.
  void StartCleanup();

 private:
  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  Profile* profile_;
  bool cleanup_started_ = false;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_DATA_SERVICE_H_
