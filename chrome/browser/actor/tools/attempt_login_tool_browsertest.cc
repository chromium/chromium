// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

using base::test::TestFuture;

namespace actor {

namespace {

actor_login::Credential MakeTestCredential(
    const std::u16string& username,
    const GURL& url,
    bool immediately_available_to_login) {
  actor_login::Credential credential;
  credential.username = username;
  // TODO(crbug.com/427171031): Clarify the format.
  credential.source_site_or_app =
      base::UTF8ToUTF16(url.GetWithEmptyPath().spec());
  credential.type = actor_login::CredentialType::kPassword;
  credential.immediatelyAvailableToLogin = immediately_available_to_login;
  return credential;
}

class MockActorLoginService : public actor_login::ActorLoginService {
 public:
  MockActorLoginService() = default;
  ~MockActorLoginService() override = default;

  void GetCredentials(tabs::TabInterface* tab,
                      actor_login::CredentialsOrErrorReply callback) override {
    std::move(callback).Run(credentials_);
  }

  void AttemptLogin(
      tabs::TabInterface* tab,
      const actor_login::Credential& credential,
      actor_login::LoginStatusResultOrErrorReply callback) override {
    last_credential_used_ = credential;
    std::move(callback).Run(login_status_);
  }

  void SetCredentials(const actor_login::CredentialsOrError& credentials) {
    credentials_ = credentials;
  }

  void SetCredential(const actor_login::Credential& credential) {
    SetCredentials(std::vector{credential});
  }

  void SetLoginStatus(actor_login::LoginStatusResultOrError login_status) {
    login_status_ = login_status;
  }

  const actor_login::Credential& last_credential_used() const {
    return last_credential_used_;
  }

 private:
  actor_login::CredentialsOrError credentials_;
  actor_login::LoginStatusResultOrError login_status_;

  actor_login::Credential last_credential_used_;
};

class ActorAttemptLoginToolTest : public ActorToolsTest {
 public:
  ActorAttemptLoginToolTest() = default;
  ~ActorAttemptLoginToolTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    execution_engine().SetActorLoginServiceForTesting(
        std::make_unique<MockActorLoginService>());
  }

  MockActorLoginService& mock_login_service() {
    return static_cast<MockActorLoginService&>(
        execution_engine().GetActorLoginService());
  }
};

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, Basic) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(u"username", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, MultipleCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url, immediately_available_to_login),
      MakeTestCredential(u"username2", url, immediately_available_to_login)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  // TODO(crbug.com/427817882): We currently just choose the first credential.
  // This test should be updated once the ability to select the credential is
  // implemented.
  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoAvailableCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/false));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsOnlyOneAvailable) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(std::vector{
      MakeTestCredential(u"username1", url,
                         /*immediately_available_to_login=*/false),
      MakeTestCredential(u"username2", url,
                         /*immediately_available_to_login=*/true),
      MakeTestCredential(u"username2", url,
                         /*immediately_available_to_login=*/false)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(u"username2", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoSigninForm) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorNoSigninForm);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, FailedAttemptLogin) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      base::unexpected(actor_login::ActorLoginError::kServiceBusy));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  TestFuture<mojom::ActionResultPtr, std::optional<size_t>> result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

}  // namespace
}  // namespace actor
