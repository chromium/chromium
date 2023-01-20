// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_
#define CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_

// This class is responsible for sending browser window data to Ash upon changes
// to foreign browser sessions.
class CrosapiSessionSyncNotifier {
 public:
  CrosapiSessionSyncNotifier(const CrosapiSessionSyncNotifier&) = delete;
  CrosapiSessionSyncNotifier() = default;
  CrosapiSessionSyncNotifier& operator=(const CrosapiSessionSyncNotifier&) =
      delete;
  ~CrosapiSessionSyncNotifier() = default;
};

#endif  // CHROME_BROWSER_LACROS_SYNC_CROSAPI_SESSION_SYNC_NOTIFIER_H_