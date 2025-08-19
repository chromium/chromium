// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor.mojom.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

using base::test::TestFuture;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace actor {

namespace {

const SkBitmap GenerateSquareBitmap(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(size, size, kOpaque_SkAlphaType));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap;
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(Profile* profile) : ExecutionEngine(profile) {}
  ~MockExecutionEngine() override = default;

  // Type alias to get around the comma in the flat_map template. MOCK_METHOD
  // breaks up the argument using commas.
  using FaviconMap = base::flat_map<GURL, gfx::Image>;

  MOCK_METHOD(void,
              PromptToSelectCredential,
              (const std::vector<actor_login::Credential>&,
               const FaviconMap&,
               ToolDelegate::CredentialSelectedCallback),
              (override));
  MOCK_METHOD(actor_login::ActorLoginService&,
              GetActorLoginService,
              (),
              (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
};

class ActorAttemptLoginToolTest : public ActorToolsTest {
 public:
  ActorAttemptLoginToolTest() {
    scoped_feature_list_.InitAndEnableFeature(
        password_manager::features::kActorLogin);
  }

  ~ActorAttemptLoginToolTest() override = default;

  void SetUpOnMainThread() override {
    ActorToolsTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());
    ASSERT_TRUE(embedded_test_server()->Start());

    ON_CALL(mock_execution_engine(), GetActorLoginService())
        .WillByDefault(ReturnRef(mock_login_service_));

    // Returns the first credential by default.
    ON_CALL(mock_execution_engine(), PromptToSelectCredential(_, _, _))
        .WillByDefault(
            [](const std::vector<actor_login::Credential>& credentials,
               const MockExecutionEngine::FaviconMap&,
               ToolDelegate::CredentialSelectedCallback callback) {
              std::move(callback).Run(credentials[0]);
            });

    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(nullptr));
  }

  std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      Profile* profile) override {
    return std::make_unique<::testing::NiceMock<MockExecutionEngine>>(profile);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

 private:
  MockActorLoginService mock_login_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(u"username", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, NoCredentials) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsSelectFirst) {
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       MultipleCredentialsSelectSecond) {
  ON_CALL(mock_execution_engine(), PromptToSelectCredential(_, _, _))
      .WillByDefault([](const std::vector<actor_login::Credential>& credentials,
                        const MockExecutionEngine::FaviconMap&,
                        ToolDelegate::CredentialSelectedCallback callback) {
        std::move(callback).Run(credentials[1]);
      });

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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username2", mock_login_service().last_credential_used().username);
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
  EXPECT_EQ(u"username2", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, OnlyUsernameFilled) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest, OnlyPasswordFilled) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kLoginNotLoginPage);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       InvalidCredentialForGivenUrl) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorInvalidCredential);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result,
                    mojom::ActionResultCode::kLoginNoCredentialsAvailable);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTest,
                       FillingNotAllowedForGivenUrl) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredential(MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true));
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorFillingNotAllowed);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
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
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kError);
}

class ActorAttemptLoginToolTestWithFaviconService
    : public ActorAttemptLoginToolTest {
 public:
  ActorAttemptLoginToolTestWithFaviconService() = default;
  ~ActorAttemptLoginToolTestWithFaviconService() override = default;

  void SetUpOnMainThread() override {
    ActorAttemptLoginToolTest::SetUpOnMainThread();
    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(&mock_favicon_service_));

    // Empty favicon by default.
    ON_CALL(mock_favicon_service_, GetFaviconImageForPageURL(_, _, _))
        .WillByDefault([](const GURL& page_url,
                          favicon_base::FaviconImageCallback callback,
                          base::CancelableTaskTracker* tracker) {
          favicon_base::FaviconImageResult result;
          result.image = gfx::Image();
          std::move(callback).Run(std::move(result));
          return static_cast<base::CancelableTaskTracker::TaskId>(1);
        });
  }

  favicon::MockFaviconService& mock_favicon_service() {
    return mock_favicon_service_;
  }

 private:
  favicon::MockFaviconService mock_favicon_service_;
};

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService, NoService) {
  ON_CALL(mock_execution_engine(), GetFaviconService())
      .WillByDefault(Return(nullptr));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL(_, _, _))
      .Times(0);

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(
      mock_execution_engine(),
      PromptToSelectCredential(/*credentials=*/credentials,
                               /*favicons=*/MockExecutionEngine::FaviconMap{},
                               /*callback=*/_));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       EmptyFavicons) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  GURL origin = url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(
      mock_execution_engine(),
      PromptToSelectCredential(/*credentials=*/credentials,
                               /*favicons=*/MockExecutionEngine::FaviconMap{},
                               /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL(origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       OneFavicon) {
  const SkBitmap bitmap = GenerateSquareBitmap(/*size=*/10, SK_ColorRED);
  auto image = gfx::Image::CreateFrom1xBitmap(bitmap);
  ON_CALL(mock_favicon_service(), GetFaviconImageForPageURL(_, _, _))
      .WillByDefault([&](const GURL& page_url,
                         favicon_base::FaviconImageCallback callback,
                         base::CancelableTaskTracker* tracker) {
        favicon_base::FaviconImageResult result;
        result.image = image;
        std::move(callback).Run(std::move(result));
        return static_cast<base::CancelableTaskTracker::TaskId>(1);
      });

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  GURL origin = url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(mock_execution_engine(),
              PromptToSelectCredential(
                  /*credentials=*/credentials,
                  /*favicons=*/MockExecutionEngine::FaviconMap{{origin, image}},
                  /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(), GetFaviconImageForPageURL(origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

IN_PROC_BROWSER_TEST_F(ActorAttemptLoginToolTestWithFaviconService,
                       TwoFavicons) {
  auto blank_icon = gfx::Image::CreateFrom1xBitmap(
      GenerateSquareBitmap(/*size=*/10, SK_ColorWHITE));
  auto link_icon = gfx::Image::CreateFrom1xBitmap(
      GenerateSquareBitmap(/*size=*/15, SK_ColorBLUE));

  const GURL blank_url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), blank_url));
  const GURL link_url =
      embedded_https_test_server().GetURL("foo.com", "/actor/link.html");

  ON_CALL(mock_favicon_service(), GetFaviconImageForPageURL(_, _, _))
      .WillByDefault([&](const GURL& page_url,
                         favicon_base::FaviconImageCallback callback,
                         base::CancelableTaskTracker* tracker) {
        favicon_base::FaviconImageResult result;
        if (page_url == blank_url.GetWithEmptyPath()) {
          result.image = blank_icon;
        } else if (page_url == link_url.GetWithEmptyPath()) {
          result.image = link_icon;
        }
        std::move(callback).Run(std::move(result));
        return static_cast<base::CancelableTaskTracker::TaskId>(1);
      });

  GURL blank_origin = blank_url.GetWithEmptyPath();
  GURL link_origin = link_url.GetWithEmptyPath();
  std::vector<actor_login::Credential> credentials =
      std::vector{MakeTestCredential(u"username1", blank_url,
                                     /*immediately_available_to_login=*/true),
                  MakeTestCredential(u"username2", link_url,
                                     /*immediately_available_to_login=*/true)};
  EXPECT_CALL(mock_execution_engine(),
              PromptToSelectCredential(
                  /*credentials=*/credentials,
                  /*favicons=*/
                  MockExecutionEngine::FaviconMap{{blank_origin, blank_icon},
                                                  {link_origin, link_icon}},
                  /*callback=*/_));
  EXPECT_CALL(mock_favicon_service(),
              GetFaviconImageForPageURL(blank_origin, _, _));
  EXPECT_CALL(mock_favicon_service(),
              GetFaviconImageForPageURL(link_origin, _, _));

  mock_login_service().SetCredentials(credentials);
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_EQ(u"username1", mock_login_service().last_credential_used().username);
}

}  // namespace
}  // namespace actor
