// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/autofill_assistant_private/autofill_assistant_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/autofill_assistant_private.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

namespace {

using autofill_assistant::ActionsResponseProto;
using autofill_assistant::MockService;
using autofill_assistant::SupportedScriptProto;
using autofill_assistant::SupportsScriptResponseProto;
using autofill_assistant::TellProto;
using base::test::RunOnceCallback;
using testing::_;
using testing::NiceMock;
using testing::StrEq;

// TODO(crbug.com/1015753): We have to split some of these tests up due to the
// MockService being owned by the controller. If we were to run two tests that
// create a controller in the same browser test, the mock service is lost and a
// real ServiceImpl is used. This is an issue in the architecture of the
// controller and service and should be changed to avoid this issue.
class AutofillAssistantPrivateApiTest : public ExtensionApiTest {
 public:
  AutofillAssistantPrivateApiTest() = default;
  AutofillAssistantPrivateApiTest(const AutofillAssistantPrivateApiTest&) =
      delete;
  AutofillAssistantPrivateApiTest& operator=(
      const AutofillAssistantPrivateApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    auto service = std::make_unique<NiceMock<MockService>>();
    mock_service_ = service.get();

    AutofillAssistantPrivateAPI::GetFactoryInstance()
        ->Get(browser()->profile())
        ->SetService(std::move(service));

    // Prepare the mock service to return two scripts when asked.
    SupportsScriptResponseProto scripts_proto;
    SupportedScriptProto* script = scripts_proto.add_scripts();
    script->set_path("some/path");
    script->mutable_presentation()->mutable_chip()->set_text("Action 0");
    SupportedScriptProto* script1 = scripts_proto.add_scripts();
    script1->set_path("some/path");
    script1->mutable_presentation()->mutable_chip()->set_text("Action 1");
    std::string scripts_output;
    scripts_proto.SerializeToString(&scripts_output);
    ON_CALL(*mock_service_, OnGetScriptsForUrl(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true, scripts_output));

    // Always return a script with a single tell action.
    ActionsResponseProto actions_proto;
    TellProto* tell_action = actions_proto.add_actions()->mutable_tell();
    tell_action->set_message("This is a test status.");
    std::string actions_output;
    actions_proto.SerializeToString(&actions_output);
    ON_CALL(*mock_service_, OnGetActions(StrEq("some/path"), _, _, _, _, _))
        .WillByDefault(RunOnceCallback<5>(true, actions_output));

    // We never return more additional actions.
    ON_CALL(*mock_service_, OnGetNextActions(_, _, _, _, _))
        .WillByDefault(RunOnceCallback<4>(true, ""));
  }

 private:
  std::unique_ptr<MockService> service_;
  MockService* mock_service_;
};

IN_PROC_BROWSER_TEST_F(AutofillAssistantPrivateApiTest, DefaultTest) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://version"));
  EXPECT_TRUE(
      RunComponentExtensionTestWithArg("autofill_assistant_private", "default"))
      << message_;
}

}  // namespace
}  // namespace extensions
