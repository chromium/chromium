// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/actor/core/actor_features.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "ui/compositor/compositor_switches.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace actor {
namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

const SkBitmap GenerateSquareBitmap(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32(size, size, kOpaque_SkAlphaType));
  bitmap.eraseColor(color);
  bitmap.setImmutable();
  return bitmap;
}

void SetCredentialRequestHandler(
    content::WebContents* glic_contents,
    int credential_index,
    webui::mojom::UserGrantedPermissionDuration permission_duration) {
  static constexpr char kHandleDialogRequest[] =
      R"js(
      (() => {
        /** Converts a PNG (Blob) to a base64 encoded string. */
        function blobToBase64(blob) {
          return new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onloadend = () => {
              resolve(reader.result);
            };
            reader.onerror = reject;
            reader.readAsDataURL(blob);
          });
        }

        async function iconGetterToDataUrl(iconGetter) {
          if (!iconGetter) {
            return undefined;
          }
          const blob = await iconGetter();
          if (!blob) {
            return undefined;
          }
          return await blobToBase64(blob);
        }

        window.credentialDialogRequestData = new Promise(resolve => {
          client.browser.selectCredentialDialogRequestHandler().subscribe(
            async (request) => {
              // Respond to the request by selecting the specified credential.
              request.onDialogClosed({
                response: {
                  taskId: request.taskId,
                  selectedCredentialId: request.credentials[$1].id,
                  permissionDuration: $2,
                }
              });

              const credentialsWithIcons = await Promise.all(
                request.credentials.map(async (cred) => {
                  const {getIcon, getAccountPicture, ...rest} = cred;
                  const icon = await iconGetterToDataUrl(getIcon);
                  const accountPicture =
                      await iconGetterToDataUrl(getAccountPicture);
                  return {...rest, icon, accountPicture};
                })
              );

              // Resolve the promise with the request data to be verified in
              // C++.
              resolve({
                taskId: request.taskId,
                showDialog: request.showDialog,
                credentials: credentialsWithIcons,
              });
            }
          );
        });
      })();
      )js";
  ASSERT_TRUE(content::ExecJs(
      glic_contents,
      content::JsReplace(kHandleDialogRequest, credential_index,
                         std::to_underlying(permission_duration))));
}

base::DictValue ExtractRequestData(content::WebContents* glic_contents) {
  static constexpr char kGetRequestData[] =
      R"js(
        (() => {
          return window.credentialDialogRequestData;
        })();
      )js";
  auto eval_result = content::EvalJs(glic_contents, kGetRequestData);
  return eval_result.ExtractDict().Clone();
}

class MockExecutionEngine : public ExecutionEngine {
 public:
  explicit MockExecutionEngine(ActorTask& task) : ExecutionEngine(task) {}
  ~MockExecutionEngine() override = default;

  MOCK_METHOD(actor_login::ActorLoginService&,
              GetActorLoginService,
              (),
              (override));
  MOCK_METHOD(favicon::FaviconService*, GetFaviconService, (), (override));
};

class AttemptLoginToolInteractiveUiTestBase
    : public InteractiveBrowserTestMixin<ActorToolsTest> {
 public:
  AttemptLoginToolInteractiveUiTestBase() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicActor,
        {{features::kGlicActorPolicyControlExemption.name, "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/441533831): We should migrate the Javascript tests to
// typescript.
class AttemptLoginToolInteractiveUiTest
    : public glic::test::InteractiveGlicTestMixin<
          AttemptLoginToolInteractiveUiTestBase>,
      public testing::WithParamInterface<bool> {
 public:
  static constexpr char kActivateSurfaceIncompatibilityNotice[] =
      "Programmatic window activation does not work on the Weston reference "
      "implementation of Wayland used on Linux testbots. It also doesn't work "
      "reliably on Linux in general. For this reason, some of these tests "
      "which "
      "use ActivateSurface() may be skipped on machine configurations which do "
      "not reliably support them.";

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    const bool federation = info.param;
    return base::StringPrintf(
        "%s", federation ? "FederationEnabled" : "FederationDisabled");
  }

  AttemptLoginToolInteractiveUiTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        password_manager::features::kActorLogin};
    std::vector<base::test::FeatureRef> disabled_features;
    if (federation_enabled()) {
      enabled_features.push_back(features::kFedCmEmbedderInitiatedLogin);
    } else {
      disabled_features.push_back(features::kFedCmEmbedderInitiatedLogin);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~AttemptLoginToolInteractiveUiTest() override = default;

  bool federation_enabled() const { return GetParam(); }

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTestMixin<
        AttemptLoginToolInteractiveUiTestBase>::SetUpOnMainThread();
    ASSERT_TRUE(embedded_https_test_server().Start());

    // Open glic window and track instance.
    RunTestSequence(OpenGlic());
    TrackGlicInstanceWithTabIndex(
        InProcessBrowserTest::browser()->tab_strip_model()->active_index());
    // Create new task with instance as ActorTaskDelegate.
    base::test::TestFuture<
        base::expected<int32_t, glic::mojom::CreateTaskErrorReason>>
        create_task_future;
    ASSERT_TRUE(GetGlicInstanceImpl());
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetGlicInstanceImpl()
                 ->GetActorTaskManager()
                 ->GetClientSessionForTesting() != nullptr;
    }));
    GetGlicInstanceImpl()
        ->GetActorTaskManager()
        ->GetClientSessionForTesting()
        ->CreateTask(actor::webui::mojom::TaskOptions::New(),
                     create_task_future.GetCallback());
    auto create_task_result = create_task_future.Get();
    ASSERT_TRUE(create_task_result.has_value());
    task_id_ = TaskId(create_task_result.value());

    ON_CALL(mock_execution_engine(), GetActorLoginService())
        .WillByDefault(ReturnRef(mock_login_service_));

    ON_CALL(mock_execution_engine(), GetFaviconService())
        .WillByDefault(Return(&mock_favicon_service_));

    ON_CALL(mock_favicon_service_, GetFaviconImageForPageURL)
        .WillByDefault([this](const GURL& page_url,
                              favicon_base::FaviconImageCallback callback,
                              base::CancelableTaskTracker* tracker) {
          favicon_base::FaviconImageResult result;
          result.image = red_image_;
          std::move(callback).Run(std::move(result));
          return static_cast<base::CancelableTaskTracker::TaskId>(1);
        });
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Must enable the pixel output. Otherwise the PNG icons will not be
    // rendered.
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    glic::test::InteractiveGlicTestMixin<
        AttemptLoginToolInteractiveUiTestBase>::SetUpCommandLine(command_line);
  }

  static std::unique_ptr<ExecutionEngine> CreateExecutionEngine(
      ActorTask& task) {
    return std::make_unique<NiceMock<MockExecutionEngine>>(task);
  }

  MockActorLoginService& mock_login_service() { return mock_login_service_; }

  MockExecutionEngine& mock_execution_engine() {
    return static_cast<MockExecutionEngine&>(execution_engine());
  }

  actor_login::Credential::Id GenerateCredentialId() {
    return credential_id_generator_.GenerateNextId();
  }

  // Note that the URL here is the bitmap encoded `red_bitmap_` image, as
  // bitmap encoding is what glic vends. I visually confirmed this is the same
  // image.
  const GURL kRedIconDataUrl{
      "data:image/bmp;base64,Qk0aAgAAAAAAAIoAAAB8AAAACgAAAPb///"
      "8BACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/"
      "yBuaVcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEA"
      "AAAAAAAAAAAAAAAAAAAAAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//"
      "wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//"
      "8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//"
      "AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//"
      "wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//"
      "8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//"
      "AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//"
      "wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//w=="};
  // Likewise for `blue_bitmap_`.
  const GURL kBlueIconDataUrl{
      "data:image/bmp;base64,Qk0aAgAAAAAAAIoAAAB8AAAACgAAAPb///"
      "8BACAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA/"
      "yBuaVcAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEA"
      "AAAAAAAAAAAAAAAAAAA/wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA/"
      "/8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//"
      "AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//"
      "wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//"
      "8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//"
      "AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//"
      "wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//"
      "8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA//8AAP//AAD//wAA/w=="};

  const gfx::Image& red_image() const { return red_image_; }
  const gfx::Image& blue_image() const { return blue_image_; }

 protected:
  MockActorLoginService mock_login_service_;
  favicon::MockFaviconService mock_favicon_service_;

 private:
  const SkBitmap red_bitmap_ = GenerateSquareBitmap(/*size=*/10, SK_ColorRED);
  const gfx::Image red_image_ = gfx::Image::CreateFrom1xBitmap(red_bitmap_);
  const SkBitmap blue_bitmap_ = GenerateSquareBitmap(/*size=*/10, SK_ColorBLUE);
  const gfx::Image blue_image_ = gfx::Image::CreateFrom1xBitmap(blue_bitmap_);

  base::test::ScopedFeatureList scoped_feature_list_;

  actor_login::Credential::Id::Generator credential_id_generator_;
  ScopedExecutionEngineFactory mock_execution_engine_factory_{
      base::BindRepeating(
          AttemptLoginToolInteractiveUiTest::CreateExecutionEngine)};
};

}  // namespace

// TODO(https://crbug.com/456675144):
// AttemptLoginToolInteractiveUiTest.SmokeTest is flaky on asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_SmokeTest DISABLED_SmokeTest
#else
#define MAYBE_SmokeTest SmokeTest
#endif
IN_PROC_BROWSER_TEST_P(AttemptLoginToolInteractiveUiTest, MAYBE_SmokeTest) {
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
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        SetCredentialRequestHandler(
            glic_contents, 1,
            webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow);
      })));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // The ActResultFuture `result` will be resolved in a RunLoop of kDefault. It
  // shouldn't be placed inside `RunTestSequence()`.
  ExpectOkResult(result);

  const std::string expected_request_origin =
      url::Origin::Create(url).Serialize();
  const std::string expected_display_origin = "example.com:12345";
  auto expected_request =
      base::DictValue()
          .Set("taskId", actor_task().id().value())
          .Set("showDialog", true)
          .Set(
              "credentials",
              base::ListValue()
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username1")
                          .Set("sourceSiteOrApp", url.GetWithEmptyPath().spec())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("icon", kRedIconDataUrl.spec())
                          .Set("type", actor_login::CredentialType::kPassword))
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username2")
                          .Set("sourceSiteOrApp", url.GetWithEmptyPath().spec())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("icon", kRedIconDataUrl.spec())
                          .Set("type",
                               actor_login::CredentialType::kPassword)));

  // Verify the dialog request content.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        const base::DictValue actual_request =
            ExtractRequestData(glic_contents);
        ASSERT_EQ(expected_request, actual_request);
      })));

  // We selected the second credential in the dialog.
  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username2", last_credential_used->username);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());
}

// TODO(https://crbug.com/456675144): Flaky on asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_HandleReauth DISABLED_HandleReauth
#else
#define MAYBE_HandleReauth HandleReauth
#endif
IN_PROC_BROWSER_TEST_P(AttemptLoginToolInteractiveUiTest, MAYBE_HandleReauth) {
#if BUILDFLAG(IS_LINUX)
  // This test does not work on Wayland, but setting SetOnIncompatibleAction
  // does not seem to skip the test.
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP();
  }
#endif

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTargetTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOtherTabId);

  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");

  actor_login::Credential persisted_cred = MakeTestCredential(
      u"username", url, /*immediately_available_to_login=*/true);
  persisted_cred.has_persistent_permission = true;
  mock_login_service().SetCredentials(std::vector{persisted_cred});
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kErrorDeviceReauthRequired);

  std::unique_ptr<ToolRequest> navigate_action =
      MakeNavigateRequest(*active_tab(), url.spec());
  std::unique_ptr<ToolRequest> login_action =
      MakeAttemptLoginRequest(*active_tab());

  ActResultFuture nav_result;

  RunTestSequence(InstrumentTab(kTargetTabId), Do([&]() {
                    actor_task().Act(ToRequestList(navigate_action),
                                     nav_result.GetCallback());
                  }));

  ExpectOkResult(nav_result);

  ActResultFuture login_result;

  // Create a new browser, and when the target tab is in the background, trigger
  // the login. As the target is in the background, the login needing reauth is
  // blocked on user attention.
  RunTestSequence(
      // clang-format off
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      InstrumentNextTab(kOtherTabId, AnyBrowser()),
      Do([&]() { chrome::NewWindow(browser()); }),
      InAnyContext(WaitForWebContentsReady(kOtherTabId)),
      Do([&]() {
        actor_task().Act(ToRequestList(login_action),
                         login_result.GetCallback());
      })
      // clang-format on
  );

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return mock_login_service().last_credential_used().has_value();
  }));

  EXPECT_FALSE(login_result.IsReady());

  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Foreground the target tab, which will retry the login.
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kActivateSurfaceIncompatibilityNotice),
      ActivateSurface(kTargetTabId));

  ExpectOkResult(login_result);
}

// TODO(https://crbug.com/456675144): Flaky on asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_MixedCredentialTypes DISABLED_MixedCredentialTypes
#else
#define MAYBE_MixedCredentialTypes MixedCredentialTypes
#endif
IN_PROC_BROWSER_TEST_P(AttemptLoginToolInteractiveUiTest,
                       MAYBE_MixedCredentialTypes) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  const bool immediately_available_to_login = true;
  mock_login_service().SetCredentials(std::vector{
      MakeTestCredentialFederated(u"username1", url),
      MakeTestCredentialFederated(u"username2", url),
      MakeTestCredential(u"username3", url, immediately_available_to_login),
      // Intentionally using the same username with a different credential type.
      MakeTestCredential(u"username1", url, immediately_available_to_login)});
  // TODO(crbug.com/486835283): Use a more meaningful status.
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Toggle the glic window.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        SetCredentialRequestHandler(
            glic_contents, 0,
            webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow);
      })));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // The ActResultFuture `result` will be resolved in a RunLoop of kDefault. It
  // shouldn't be placed inside `RunTestSequence()`.
  ExpectOkResult(result);

  const std::string expected_request_origin =
      url::Origin::Create(url).Serialize();
  const std::string expected_display_origin = "example.com:12345";
  auto expected_request =
      base::DictValue()
          .Set("taskId", actor_task().id().value())
          .Set("showDialog", true)
          .Set(
              "credentials",
              base::ListValue()
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username1")
                          .Set("sourceSiteOrApp", url.host())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("type", actor_login::CredentialType::kFederated))
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username2")
                          .Set("sourceSiteOrApp", url.host())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("type", actor_login::CredentialType::kFederated))
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username3")
                          .Set("sourceSiteOrApp", url.GetWithEmptyPath().spec())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("icon", kRedIconDataUrl.spec())
                          .Set("type", actor_login::CredentialType::kPassword))
                  .Append(
                      base::DictValue()
                          .Set("id", GenerateCredentialId().value())
                          .Set("username", "username1")
                          .Set("sourceSiteOrApp", url.GetWithEmptyPath().spec())
                          .Set("requestOrigin", expected_request_origin)
                          .Set("displayOrigin", expected_display_origin)
                          .Set("icon", kRedIconDataUrl.spec())
                          .Set("type",
                               actor_login::CredentialType::kPassword)));

  // Verify the dialog request content.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        const auto actual_request = ExtractRequestData(glic_contents);
        ASSERT_EQ(expected_request, actual_request);
      })));

  // We selected the first credential in the dialog.
  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
  EXPECT_EQ(actor_login::CredentialType::kFederated,
            last_credential_used->type);
  EXPECT_TRUE(mock_login_service().last_permission_was_permanent());
}

// TODO(https://crbug.com/456675144): Flaky on asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_FederatedAccountPicture DISABLED_FederatedAccountPicture
#else
#define MAYBE_FederatedAccountPicture FederatedAccountPicture
#endif
IN_PROC_BROWSER_TEST_P(AttemptLoginToolInteractiveUiTest,
                       MAYBE_FederatedAccountPicture) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  actor_login::Credential cred1 =
      MakeTestCredentialFederated(u"username1", url);
  actor_login::Credential cred2 =
      MakeTestCredentialFederated(u"username2", url);
  cred1.federation_detail->account_picture = blue_image();
  cred2.federation_detail->account_picture = red_image();
  cred1.federation_detail->brand_icon = red_image();
  cred2.federation_detail->brand_icon = cred1.federation_detail->brand_icon;

  mock_login_service().SetCredentials(std::vector{cred1, cred2});
  // TODO(crbug.com/486835283): Use a more meaningful status.
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  // Toggle the glic window.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        SetCredentialRequestHandler(
            glic_contents, 0,
            webui::mojom::UserGrantedPermissionDuration::kAlwaysAllow);
      })));

  std::unique_ptr<ToolRequest> action = MakeAttemptLoginRequest(*active_tab());
  ActResultFuture result;
  actor_task().Act(ToRequestList(action), result.GetCallback());
  // The ActResultFuture `result` will be resolved in a RunLoop of kDefault. It
  // shouldn't be placed inside `RunTestSequence()`.
  ExpectOkResult(result);

  const std::string expected_request_origin =
      url::Origin::Create(url).Serialize();
  const std::string expected_display_origin = "example.com:12345";
  auto expected_request =
      base::DictValue()
          .Set("taskId", actor_task().id().value())
          .Set("showDialog", true)
          .Set("credentials",
               base::ListValue()
                   .Append(base::DictValue()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username1")
                               .Set("sourceSiteOrApp", url.host())
                               .Set("requestOrigin", expected_request_origin)
                               .Set("displayOrigin", expected_display_origin)
                               .Set("accountPicture", kBlueIconDataUrl.spec())
                               .Set("icon", kRedIconDataUrl.spec())
                               .Set("type",
                                    actor_login::CredentialType::kFederated))
                   .Append(base::DictValue()
                               .Set("id", GenerateCredentialId().value())
                               .Set("username", "username2")
                               .Set("sourceSiteOrApp", url.host())
                               .Set("requestOrigin", expected_request_origin)
                               .Set("displayOrigin", expected_display_origin)
                               .Set("accountPicture", kRedIconDataUrl.spec())
                               .Set("icon", kRedIconDataUrl.spec())
                               .Set("type",
                                    actor_login::CredentialType::kFederated)));

  // Verify the dialog request content.
  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [&](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        const base::DictValue actual_request =
            ExtractRequestData(glic_contents);
        ASSERT_EQ(expected_request, actual_request);
      })));

  const auto& last_credential_used =
      mock_login_service().last_credential_used();
  ASSERT_TRUE(last_credential_used.has_value());
  EXPECT_EQ(u"username1", last_credential_used->username);
  EXPECT_EQ(actor_login::CredentialType::kFederated,
            last_credential_used->type);
}

// TODO(https://crbug.com/456675144): Flaky on asan.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PopupNavigation DISABLED_PopupNavigation
#else
#define MAYBE_PopupNavigation PopupNavigation
#endif
IN_PROC_BROWSER_TEST_P(AttemptLoginToolInteractiveUiTest,
                       MAYBE_PopupNavigation) {
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/actor/blank.html");
  const GURL popup_url =
      embedded_https_test_server().GetURL("example.com", "/actor/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  mock_login_service().SetCredentials(
      std::vector{MakeTestCredentialFederated(u"username1", url)});
  // TODO(crbug.com/486835283): Use a more meaningful status.
  mock_login_service().SetLoginStatus(
      actor_login::LoginStatusResult::kSuccessUsernameAndPasswordFilled);

  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [](::ui::TrackedElement* el) {
        static constexpr char kHandleDialogRequest[] =
            R"js(
        (() => {
          window.awaitCredentialSelection = new Promise(resolve => {
            client.browser.selectCredentialDialogRequestHandler().subscribe(
                (request) => {
                  // Intentionally never reply.

                  // Continue the test while an attempt login is in progress.
                  resolve();
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
  ActResultFuture dont_care;
  actor_task().Act(ToRequestList(action), dont_care.GetCallback());

  RunTestSequence(InAnyContext(WithElement(
      glic::test::kGlicContentsElementId, [](::ui::TrackedElement* el) {
        content::WebContents* glic_contents =
            AsInstrumentedWebContents(el)->web_contents();
        EXPECT_TRUE(
            content::ExecJs(glic_contents, "window.awaitCredentialSelection;"));
      })));

  // The timing of the following is contrived for ease of testing.
  // Realistically, the auth window would only be triggered after the credential
  // selection has completed.
  //
  // While an attempt login is ongoing, trigger a popup window. When federation
  // is enabled, allow the popup. If disabled, preserve the popup behaviour of
  // other tools. See also ExecutionEngineBrowserTest.ForceSameTabNavigation.

  content::WebContentsAddedObserver web_contents_added_observer;
  EXPECT_EQ(
      federation_enabled(),
      content::EvalJs(web_contents(),
                      content::JsReplace(
                          "!!window.open($1, 'my_cool_auth_window', 'popup');",
                          popup_url)));

  if (federation_enabled()) {
    content::WebContents* new_contents =
        web_contents_added_observer.GetWebContents();
    EXPECT_EQ(true, content::EvalJs(new_contents, "!!window.opener;"));
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// FederationEnabled tests are flaky on ChromeOS.
// TODO(b/495445228): Fix flaky tests.
INSTANTIATE_TEST_SUITE_P(All,
                         AttemptLoginToolInteractiveUiTest,
                         testing::Values(false),
                         AttemptLoginToolInteractiveUiTest::DescribeParams);
#else
INSTANTIATE_TEST_SUITE_P(All,
                         AttemptLoginToolInteractiveUiTest,
                         testing::Bool(),
                         AttemptLoginToolInteractiveUiTest::DescribeParams);
#endif

}  // namespace actor
