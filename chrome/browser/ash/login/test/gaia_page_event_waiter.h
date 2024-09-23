// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_GAIA_PAGE_EVENT_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_GAIA_PAGE_EVENT_WAITER_H_

#include <string>

#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

// Helper class to wait for a given `event` from authenticator hosting Gaia
// on the login webui. `authenticator_id` is the JS code to access the
// authenticator instance. There are two instances in the login webui:
//   $('gaia-signin').authenticator for the gaia login screen
//   $('enterprise-enrollment').authenticator for enrollment screen
class GaiaPageEventWaiter : public test::TestConditionWaiter {
 public:
  GaiaPageEventWaiter(const std::string& authenticator_id,
                      const std::string& event);
  ~GaiaPageEventWaiter() override;

  // test::TestConditionWaiter:
  void Wait() override;

 private:
  content::DOMMessageQueue message_queue_;
  bool wait_called_ = false;
  std::string event_done_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_GAIA_PAGE_EVENT_WAITER_H_
