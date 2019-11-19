// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_

#include <string>

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/base/model_type.h"

class Profile;

namespace syncer {
class ProfileSyncService;
}  // namespace syncer

// Sets a custom theme and wait until the asynchronous process is done.
void SetCustomTheme(Profile* profile, int theme_index = 0);

// Checker to block until the server has a given number of entities.
class ServerCountMatchStatusChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  ServerCountMatchStatusChecker(syncer::ModelType type, size_t count);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const syncer::ModelType type_;
  const size_t count_;
};

// Checker to block until service is waiting for a passphrase.
class PassphraseRequiredChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseRequiredChecker(syncer::ProfileSyncService* service);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

// Checker to block until service has accepted a new passphrase.
class PassphraseAcceptedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit PassphraseAcceptedChecker(syncer::ProfileSyncService* service);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
