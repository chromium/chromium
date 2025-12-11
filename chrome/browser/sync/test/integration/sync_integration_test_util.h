// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/base/data_type.h"

class Profile;

// Sets a custom theme and wait until the asynchronous process is done.
void SetCustomTheme(Profile* profile, int theme_index = 0);

// Checker to block until the server has a given number of entities.
class ServerCountMatchStatusChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  ServerCountMatchStatusChecker(syncer::DataType type, size_t count);

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const syncer::DataType type_;
  const size_t count_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_INTEGRATION_TEST_UTIL_H_
