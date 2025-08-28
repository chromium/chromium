// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"

namespace actor {
namespace {

using ::testing::NiceMock;
using ::testing::ReturnRef;

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(Profile* profile) : ExecutionEngine(profile) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(actor_login::ActorLoginService&,
              GetActorLoginService,
              (),
              (override));
};

using AttemptLoginToolInteractiveUiTestBase =
    InteractiveBrowserTestT<ActorToolsTest>;

// TODO(crbug.com/441533831): We should migrate the Javascript tests to
// typescript.
class AttemptLoginToolInteractiveUiTest
    : public glic::test::InteractiveGlicTestT<
          AttemptLoginToolInteractiveUiTestBase> {
 public:
  AttemptLoginToolInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kActorLogin);
  }
  ~AttemptLoginToolInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTestT<
        AttemptLoginToolInteractiveUiTestBase>::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());

    ON_CALL(mock_execution_engine(), GetActorLoginService())
        .WillByDefault(ReturnRef(mock_login_service_));
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile) override {
    return std::make_unique<NiceMock<MockExecutionEngine>>(profile);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  actor_login::Credential::Id GenerateCredentialId() {
    return credential_id_generator_.GenerateNextId();
  }

 private:
  MockActorLoginService mock_login_service_;
  base::test::ScopedFeatureList scoped_feature_list_;

  actor_login::Credential::Id::Generator credential_id_generator_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AttemptLoginToolInteractiveUiTest, SmokeTest) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login),
      MakeTestCredential(u"username2", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Toggle the glic window.
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached),
      InAnyContext(WithElement(
          glic::test::kGlicContentsElementId,
          [](::ui::TrackedElement* el) mutable {
            static constexpr char kHandleDialogRequest[] =
                R"js(
      (() => {
        window.credentialDialogRequestData = new Promise(resolve => {
          client.browser.selectCredentialDialogRequestHandler().subscribe(
            request => {
              // Respond to the request by selecting the second credential.
              request.onDialogClosed({
                response: {
                  taskId: request.taskId,
                  selectedCredentialId: request.credentials[1].id,
                }
              });
              // Resolve the promise with the request data to be verified in
              // C++.
              resolve({
                taskId: request.taskId,
                showDialog: request.showDialog,
                credentials: request.credentials,
              });
            }
          );
        });
      })();
              )js";
            content::WebContents* glic_contents =
                AsInstrumentedWebContents(el)->web_contents();
            ASSERT_TRUE(content::ExecJs(glic_contents, kHandleDialogRequest));
          })));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // The ActResultFuture `result` will be resolved in a RunLoop of kDefault. It
  // shouldn't be placed inside `RunTestSequence()`.
  ExpectOkResult(result);

  auto expected_request =
      base::Value::Dict()
          .Set("taskId", actor_task().id().value())
          .Set("showDialog", true)
          .Set("credentials",
               base::Value::List()
                   .Append(base::Value::Dict()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username1")
                               .Set("sourceSiteOrApp",
                                    url.GetWithEmptyPath().spec()))
                   .Append(base::Value::Dict()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username2")
                               .Set("sourceSiteOrApp",
                                    url.GetWithEmptyPath().spec())));

  // Verify the dialog request content.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        static constexpr char kGetRequestData[] =
            R"js(
              (() => {
                return window.credentialDialogRequestData;
              })();
            )js";
        auto eval_result = content::EvalJs(glic_contents, kGetRequestData);
        const auto& actual_request = eval_result.ExtractDict();
        ASSERT_EQ(expected_request, actual_request);
      })));

  // We selected the second credential in the dialog.
  EXPECT_EQ(u"username2", mock_login_service().last_credential_used().username);
}

}  // namespace actor
