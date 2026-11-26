// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/declarative_api.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/test/base/testing_profile.h"
#include "components/guest_view/buildflags/buildflags.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

constexpr char kAddContentArgs[] = R"([
  "declarativeContent.onPageChanged", 0, [{
    "id": "r1",
    "conditions": [{"instanceType": "declarativeContent.PageStateMatcher",
                    "pageUrl": {"hostSuffix": "example.com"}}],
    "actions": [{"instanceType": "declarativeContent.ShowAction"}]
  }]
])";
constexpr char kGetContentArgs[] =
    R"(["declarativeContent.onPageChanged", 0, null])";
constexpr char kRemoveContentArgs[] =
    R"(["declarativeContent.onPageChanged", 0, ["r1"]])";

}  // namespace

// Lives in //chrome/browser/extensions so `ChromeExtensionsClient` registers
// the `declarativeContent` API/permissions across Desktop and Desktop Android.
class DeclarativeApiUnitTest : public testing::Test {
 public:
  DeclarativeApiUnitTest() = default;
  ~DeclarativeApiUnitTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    RulesRegistryService::GetFactoryInstance()->SetTestingFactoryAndUse(
        profile_.get(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<RulesRegistryService>(context);
        }));

    authorized_ext_ = ExtensionBuilder("Authorized Extension")
                          .SetManifestVersion(2)
                          .SetAction(ActionInfo::Type::kPage)
                          .AddAPIPermission("declarativeContent")
                          .AddHostPermission("<all_urls>")
                          .Build();
    ExtensionRegistry::Get(profile_.get())->AddEnabled(authorized_ext_);
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    web_contents_.reset();
    profile_.reset();
  }

 protected:
  ExtensionFunction::ResponseType RunRulesFunction(
      const char* function_name,
      const Extension* extension,
      mojom::ContextType context_type,
      const char* args_json,
      content::RenderFrameHost* rfh,
      std::optional<WorkerId> worker_id = std::nullopt) {
    ExtensionFunctionDispatcher dispatcher(profile_.get());
    scoped_refptr<ExtensionFunction> fn =
        ExtensionFunctionRegistry::GetInstance().NewFunction(function_name);
    CHECK(fn);
    fn->set_extension(extension);
    fn->set_source_context_type(context_type);
    fn->SetArgs(base::test::ParseJsonList(args_json));
    if (worker_id) {
      fn->set_worker_id(*worker_id);
      fn->set_source_process_id(worker_id->render_process_id.GetUnsafeValue());
    } else if (rfh) {
      fn->SetRenderFrameHost(rfh);
      fn->set_source_process_id(rfh->GetProcess()->GetDeprecatedID());
    }
    fn->SetDispatcher(dispatcher.AsWeakPtr());
    fn->set_has_callback(true);
    base::test::TestFuture<ExtensionFunction::ResponseType, base::ListValue,
                           const std::string&, mojom::ExtraResponseDataPtr>
        future;
    fn->set_response_callback(future.GetCallback());
    fn->RunWithValidation().Execute();
    return std::get<0>(future.Take());
  }

  TestingProfile* profile() { return profile_.get(); }
  const Extension* authorized_ext() const { return authorized_ext_.get(); }
  content::RenderFrameHost* frame() {
    return web_contents_->GetPrimaryMainFrame();
  }
  content::MockRenderProcessHost* rph() {
    return static_cast<content::MockRenderProcessHost*>(frame()->GetProcess());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<const Extension> authorized_ext_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Verifies that `RulesFunction::Run()` rejects calls with a null `extension_`
// or from non-privileged contexts with `kBadMessage`.
TEST_F(DeclarativeApiUnitTest, RulesFunctionRejectsNonPrivilegedContexts) {
  EXPECT_EQ(ExtensionFunction::ResponseType::kBadMessage,
            RunRulesFunction("events.addRules", nullptr,
                             mojom::ContextType::kPrivilegedExtension,
                             kAddContentArgs, frame()));
  for (mojom::ContextType ctx :
       {mojom::ContextType::kContentScript, mojom::ContextType::kUserScript,
        mojom::ContextType::kUnprivilegedExtension,
        mojom::ContextType::kOffscreenExtension}) {
    EXPECT_EQ(ExtensionFunction::ResponseType::kBadMessage,
              RunRulesFunction("events.addRules", authorized_ext(), ctx,
                               kAddContentArgs, frame()));
  }
}

// Verifies that `RulesFunction::Run()` rejects malformed schema arguments,
// unknown or listener-only event names, negative or mismatched
// `web_view_instance_id` values, missing `webview` permission, and frameless or
// Service Worker `<webview>` calls with `kBadMessage` without creating a
// `<webview>` rules registry.
TEST_F(DeclarativeApiUnitTest,
       RulesFunctionRejectsMalformedEventAndWebViewParams) {
  scoped_refptr<const Extension> webview_app =
      ExtensionBuilder("WebView App", ExtensionBuilder::Type::PLATFORM_APP)
          .AddAPIPermission("webview")
          .Build();
  ExtensionRegistry::Get(profile())->AddEnabled(webview_app);

  constexpr const char* kBadFrameGetArgs[] = {
      R"([])",
      R"([123, "not_int"])",
      R"(["tabs.onUpdated", 0, null])",
      R"(["declarativeContent.unknownEvent", 0, null])",
      R"(["declarativeWebRequest.onMessage", 0, null])",
      R"(["webViewInternal.declarativeWebRequest.onMessage", 1, null])",
      R"(["webViewInternal.declarativeWebRequest.onRequest", 0, null])",
      R"(["declarativeWebRequest.onRequest", 1, null])",
      R"(["declarativeContent.onPageChanged", 1, null])",
      R"(["declarativeContent.onPageChanged", -1, null])",
      // Live frame (`rfh != nullptr`), but `authorized_ext()` lacks `kWebView`:
      R"(["webViewInternal.declarativeWebRequest.onRequest", 1, null])",
  };
  for (const char* bad_args : kBadFrameGetArgs) {
    SCOPED_TRACE(bad_args);
    const int bad_before = rph()->bad_msg_count();
    EXPECT_EQ(ExtensionFunction::ResponseType::kBadMessage,
              RunRulesFunction("events.getRules", authorized_ext(),
                               mojom::ContextType::kPrivilegedExtension,
                               bad_args, frame()));
    EXPECT_EQ(bad_before + 1, rph()->bad_msg_count());
  }

  // Frameless and Service-Worker `<webview>` calls fail with `kBadMessage` even
  // when the caller (`webview_app`) holds `APIPermissionID::kWebView`.
  constexpr char kWebViewGetArgs[] =
      R"(["webViewInternal.declarativeWebRequest.onRequest", 1, null])";
  EXPECT_EQ(ExtensionFunction::ResponseType::kBadMessage,
            RunRulesFunction("events.getRules", webview_app.get(),
                             mojom::ContextType::kPrivilegedExtension,
                             kWebViewGetArgs, /*rfh=*/nullptr));
  EXPECT_EQ(ExtensionFunction::ResponseType::kBadMessage,
            RunRulesFunction("events.getRules", webview_app.get(),
                             mojom::ContextType::kPrivilegedExtension,
                             kWebViewGetArgs, /*rfh=*/nullptr,
                             WorkerId{webview_app->id(), rph()->GetID(),
                                      /*version_id=*/1, /*thread_id=*/1}));
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  EXPECT_FALSE(RulesRegistryService::Get(profile())->HasRulesRegistryForTesting(
      WebViewGuest::GetOrGenerateRulesRegistryID(rph()->GetDeprecatedID(), 1),
      declarative_webrequest_constants::kOnRequest));
  MenuManagerFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindRepeating(&MenuManagerFactory::BuildServiceInstanceForTesting));
  WebViewGuest::CleanUp(profile(), rph()->GetID(), 1);
#endif
}

// Verifies that `RulesFunction::Run()` checks target event availability via
// `ExtensionAPI::IsAvailable()` — returning a normal `kFailed` error (without
// killing the renderer or recording UMA) when the extension lacks the target
// API permission or the rules registry is unavailable, and succeeding
// (recording UMA) when authorized.
TEST_F(DeclarativeApiUnitTest, RulesFunctionEnforcesEventAvailability) {
  scoped_refptr<const Extension> unauthorized_ext =
      ExtensionBuilder("Unauthorized Extension").SetManifestVersion(3).Build();
  ExtensionRegistry::Get(profile())->AddEnabled(unauthorized_ext);
  base::HistogramTester histograms;

  // Missing target event permission returns `kFailed` without killing the
  // renderer or recording UMA.
  const int bad_before = rph()->bad_msg_count();
  EXPECT_EQ(ExtensionFunction::ResponseType::kFailed,
            RunRulesFunction("events.addRules", unauthorized_ext.get(),
                             mojom::ContextType::kPrivilegedExtension,
                             kAddContentArgs, frame()));
  EXPECT_EQ(bad_before, rph()->bad_msg_count());
  histograms.ExpectTotalCount("Extensions.DeclarativeAPIFunctionCalls", 0);

  // Authorized privileged caller succeeds and records UMA.
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            RunRulesFunction("events.addRules", authorized_ext(),
                             mojom::ContextType::kPrivilegedExtension,
                             kAddContentArgs, frame()));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            RunRulesFunction("events.getRules", authorized_ext(),
                             mojom::ContextType::kPrivilegedExtension,
                             kGetContentArgs, frame()));
  EXPECT_EQ(ExtensionFunction::ResponseType::kSucceeded,
            RunRulesFunction("events.removeRules", authorized_ext(),
                             mojom::ContextType::kPrivilegedExtension,
                             kRemoveContentArgs, frame()));
  histograms.ExpectTotalCount("Extensions.DeclarativeAPIFunctionCalls", 3);

  // Unavailable `rules_registry_` (e.g. after `RulesRegistryService` shutdown)
  // returns `kFailed` rather than killing a well-behaved renderer.
  RulesRegistryService::Get(profile())->Shutdown();
  EXPECT_EQ(ExtensionFunction::ResponseType::kFailed,
            RunRulesFunction("events.getRules", authorized_ext(),
                             mojom::ContextType::kPrivilegedExtension,
                             kGetContentArgs, frame()));
  EXPECT_EQ(bad_before, rph()->bad_msg_count());
  histograms.ExpectTotalCount("Extensions.DeclarativeAPIFunctionCalls", 3);
}

}  // namespace extensions
