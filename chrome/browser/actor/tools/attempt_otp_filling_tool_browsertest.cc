// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"

#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "content/public/test/browser_test.h"

namespace actor {

namespace {

class AttemptOtpFillingToolBrowserTest : public ActorToolsTest {};

// The tool can be created and returns OK.
IN_PROC_BROWSER_TEST_F(AttemptOtpFillingToolBrowserTest,
                       ToolGetsCreatedAndReturnsOk) {
  std::unique_ptr<ToolRequest> request =
      std::make_unique<AttemptOtpFillingToolRequest>(active_tab()->GetHandle());

  ActResultFuture result;
  actor_task().Act(ToRequestList(std::move(request)), result.GetCallback());

  ExpectOkResult(result);
}

}  // namespace
}  // namespace actor
