// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/gaia_page_event_waiter.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"

namespace ash {

GaiaPageEventWaiter::GaiaPageEventWaiter(const std::string& authenticator_id,
                                         const std::string& event)
    : message_queue_(LoginDisplayHost::default_host()->GetOobeWebContents()) {
  std::string js =
      R"((function() {
            var authenticator = $AuthenticatorId;
            var f = function() {
              authenticator.removeEventListener('$Event', f);
              window.domAutomationController.send('$Done');
            };
            authenticator.addEventListener('$Event', f);
          })();)";
  base::ReplaceSubstringsAfterOffset(&js, 0, "$AuthenticatorId",
                                     authenticator_id);
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Event", event);

  event_done_ = base::StrCat({event, "_Done"});
  base::ReplaceSubstringsAfterOffset(&js, 0, "$Done", event_done_);

  test::OobeJS().Evaluate(js);
}

GaiaPageEventWaiter::~GaiaPageEventWaiter() {
  EXPECT_TRUE(wait_called_);
}

void GaiaPageEventWaiter::Wait() {
  ASSERT_FALSE(wait_called_) << "Wait should be called once";
  wait_called_ = true;
  std::string target_message = base::StrCat({"\"", event_done_, "\""});
  std::string message;
  do {
    ASSERT_TRUE(message_queue_.WaitForMessage(&message));
  } while (message != target_message);
}

}  // namespace ash
