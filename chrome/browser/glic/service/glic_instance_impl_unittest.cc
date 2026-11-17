// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_instance_impl.h"

#include <memory>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/provider_features.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service_factory.h"
#include "chrome/browser/glic/test_support/fake_web_contents_manager.h"
#include "chrome/browser/glic/test_support/glic_api_test_passkeys.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

TEST(GlicInstanceImplTest, IsTutorialMarketingUrl_PromotionPage) {
  auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss#gemini"),
      *spec));
  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome"
           "?source=zss"),
      *spec));
  EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?utm_medium=cta&source=zss#gemini"),
      *spec));

  // Missing or mismatched source query parameter.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=other"),
      *spec));

  // Requires HTTPS scheme.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("http://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss"),
      *spec));

  // Requires exact www.google.com host and exact path.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://sites.google.com/view/foo/chrome/ai-innovations/"
           "gemini-in-chrome?source=zss"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/intl/en/chrome/ai-innovations/"
           "gemini-in-chrome/?source=zss"),
      *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "extra/?source=zss"),
      *spec));

  // Invalid and non-HTTP(S) URLs.
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(GURL(), *spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("chrome://settings/?source=zss"), *spec));
}

TEST(GlicInstanceImplTest, IsTutorialMarketingUrl_ProviderSpecAllowlist) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicMarketingAutoOpen,
        {{features::kGlicMarketingAutoOpenAllowlistedUrlsParam,
          "https://example.com/tutorial/, http://127.0.0.1:8080/title1.html"}});
    auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial?source=zss#section"), *spec));
    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://127.0.0.1:8080/title1.html/?utm_source=x&source=zss"),
        *spec));

    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://example.com/tutorial?source=zss"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("http://127.0.0.1:9090/title1.html?source=zss"), *spec));
    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/other?source=zss"), *spec));
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kGlicMarketingAutoOpen);
    auto spec = CreateGlicProviderSpec(GlicProvider::kGemini, {});

    EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://example.com/tutorial?source=zss"), *spec));
    EXPECT_TRUE(GlicInstanceImpl::IsTutorialMarketingUrl(
        GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
             "?source=zss#gemini"),
        *spec));
  }
}

TEST(GlicInstanceImplTest,
     IsTutorialMarketingUrl_DisabledWithoutGeminiAccountEligibility) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kGlicMarketingAutoOpen,
      {{features::kGlicMarketingAutoOpenAllowlistedUrlsParam,
        "https://example.com/tutorial"}});
  auto geic_spec = CreateGlicProviderSpec(GlicProvider::kGeminiEnterprise, {});

  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://www.google.com/chrome/ai-innovations/gemini-in-chrome/"
           "?source=zss#gemini"),
      *geic_spec));
  EXPECT_FALSE(GlicInstanceImpl::IsTutorialMarketingUrl(
      GURL("https://example.com/tutorial?source=zss"), *geic_spec));
}

class TestGlicInstanceCoordinatorImpl : public GlicInstanceCoordinatorImpl {
 public:
  TestGlicInstanceCoordinatorImpl(Profile* profile,
                                  GlicKeyedService* service,
                                  content::TestWebContentsFactory* factory)
      : GlicInstanceCoordinatorImpl(profile,
                                    /*identity_manager=*/nullptr,
                                    service,
                                    &service->enabling(),
                                    /*contextual_cueing_service=*/nullptr),
        factory_(factory) {
    SetWarmingEnabledForTesting(false);
  }

  // GlicInstanceCoordinatorImpl:
  std::unique_ptr<GlicWebContentsManager> CreateWebContentsManager() override {
    return std::make_unique<FakeWebContentsManager>(
        factory_->CreateWebContents(GetProfile()));
  }

 private:
  raw_ptr<content::TestWebContentsFactory> factory_;
};

class GlicInvocationHandleTest : public testing::Test {
 protected:
  using ConversationTarget = std::
      variant<DefaultConversation, NewConversation, ConversationId, InstanceId>;

  struct TestTab {
    TestTab(TestingProfile* profile, content::TestWebContentsFactory* factory)
        : contents(factory->CreateWebContents(profile)), helper(&tab) {
      ON_CALL(tab, GetContents()).WillByDefault(testing::Return(contents));
      ON_CALL(tab, GetProfile()).WillByDefault(testing::Return(profile));
    }

    tabs::MockTabInterface tab;
    raw_ptr<content::WebContents> contents = nullptr;
    GlicInstanceHelper helper;
  };

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    TestingProfile::TestingFactories testing_factories;
    testing_factories.emplace_back(
        GlicKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&GlicInvocationHandleTest::CreateGlicKeyedService,
                            base::Unretained(this)));
    profile_ = profile_manager_.CreateTestingProfile(
        "test", std::move(testing_factories));
    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();
    GlicKeyedService* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(profile_, /*create=*/true);
    ASSERT_TRUE(service);
    coordinator_ = std::make_unique<TestGlicInstanceCoordinatorImpl>(
        profile_, service, web_contents_factory_.get());
    primary_tab_ =
        std::make_unique<TestTab>(profile_, web_contents_factory_.get());
    secondary_tab_ =
        std::make_unique<TestTab>(profile_, web_contents_factory_.get());
  }

  std::unique_ptr<KeyedService> CreateGlicKeyedService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<testing::NiceMock<MockGlicKeyedService>>(
        profile, IdentityManagerFactory::GetForProfile(profile),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_,
        ContextualCueingServiceFactory::GetForProfile(profile),
        actor::ActorKeyedServiceFactory::GetActorKeyedService(profile));
  }

  void TearDown() override {
    if (coordinator_) {
      coordinator_->Shutdown();
      coordinator_.reset();
    }
    secondary_tab_.reset();
    primary_tab_.reset();
    web_contents_factory_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  GlicInvocationHandle StartHiddenClientInvoke(
      tabs::TabInterface& tab,
      ConversationTarget conversation,
      bool supersede_if_in_progress,
      base::OnceCallback<void(GlicInvokeError)> on_error) {
    GlicInvokeOptions options(mojom::InvocationSource::kActorTaskIcon);
    options.target = Target(tab, std::move(conversation));
    options.target.actuation_target = mojom::ActuationTarget::kTargetSurface;
    options.prompts = {"test prompt"};
    options.supersede_if_in_progress = supersede_if_in_progress;
    options.on_error = std::move(on_error);

    GlicInvokeWithAutoSubmitOptions auto_submit_options;
    auto_submit_options.show_panel = false;

    return coordinator_->InvokeWithAutoSubmit(GlicApiTestPasskeys::GetPassKey(),
                                              std::move(options),
                                              std::move(auto_submit_options));
  }

  content::BrowserTaskEnvironment task_environment_;
  GlicEnabling::ScopedBypassEnablementChecksForTesting bypass_enablement_;
  GlicProfileManager glic_profile_manager_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<TestGlicInstanceCoordinatorImpl> coordinator_;
  std::unique_ptr<TestTab> primary_tab_;
  std::unique_ptr<TestTab> secondary_tab_;
};

TEST_F(GlicInvocationHandleTest, CancelCancelsActiveInvocationAndRepeatIsNoOp) {
  base::test::TestFuture<GlicInvokeError> error_future;

  GlicInvocationHandle handle = StartHiddenClientInvoke(
      primary_tab_->tab, NewConversation{},
      /*supersede_if_in_progress=*/false, error_future.GetCallback());
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle.instance());
  EXPECT_TRUE(handle.instance()->IsInvoking());

  GlicInvocationHandle copied_handle = handle;
  handle.Cancel();
  EXPECT_FALSE(handle.instance()->IsInvoking());
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kCancelled);

  // Repeating `Cancel()` on the same handle or a copy after cancellation is a
  // safe no-op.
  handle.Cancel();
  copied_handle.Cancel();
  EXPECT_FALSE(handle.instance()->IsInvoking());
}

TEST_F(GlicInvocationHandleTest,
       CancellingOrDestroyingHandleDoesNotAffectOtherInvocations) {
  base::test::TestFuture<GlicInvokeError> first_error;
  base::test::TestFuture<GlicInvokeError> second_error;

  GlicInvocationHandle first_handle = StartHiddenClientInvoke(
      primary_tab_->tab, NewConversation{},
      /*supersede_if_in_progress=*/false, first_error.GetCallback());
  ASSERT_TRUE(first_handle);
  EXPECT_TRUE(first_handle.instance()->IsInvoking());

  base::WeakPtr<GlicInstance> second_instance;
  {
    GlicInvocationHandle second_handle = StartHiddenClientInvoke(
        secondary_tab_->tab, NewConversation{},
        /*supersede_if_in_progress=*/false, second_error.GetCallback());
    ASSERT_TRUE(second_handle);
    second_instance = second_handle.instance();
    EXPECT_TRUE(second_instance->IsInvoking());
    // Leaving scope destroys `second_handle` without cancelling its invocation.
  }

  GlicInvocationHandle empty_handle;
  GlicInvocationHandle null_handle = nullptr;
  GlicInvocationHandle mock_style_handle = second_instance;
  EXPECT_FALSE(empty_handle);
  EXPECT_FALSE(null_handle);
  EXPECT_TRUE(mock_style_handle);
  empty_handle.Cancel();
  null_handle.Cancel();
  mock_style_handle.Cancel();
  first_handle.Cancel();

  EXPECT_EQ(first_error.Get(), GlicInvokeError::kCancelled);
  ASSERT_TRUE(second_instance);
  EXPECT_TRUE(second_instance->IsInvoking());
  EXPECT_FALSE(second_error.IsReady());

  second_instance->CancelInvoke();
  EXPECT_FALSE(second_instance->IsInvoking());
  EXPECT_EQ(second_error.Get(), GlicInvokeError::kCancelled);
}

TEST_F(GlicInvocationHandleTest,
       SupersededHandleDoesNotCancelReplacementInvocation) {
  base::test::TestFuture<GlicInvokeError> first_error;
  base::test::TestFuture<GlicInvokeError> second_error;

  GlicInvocationHandle first_handle = StartHiddenClientInvoke(
      primary_tab_->tab, ConversationId{"conv-1"},
      /*supersede_if_in_progress=*/false, first_error.GetCallback());
  ASSERT_TRUE(first_handle);
  EXPECT_TRUE(first_handle.instance()->IsInvoking());

  GlicInvocationHandle second_handle = StartHiddenClientInvoke(
      primary_tab_->tab, ConversationId{"conv-1"},
      /*supersede_if_in_progress=*/true, second_error.GetCallback());
  ASSERT_TRUE(second_handle);
  EXPECT_EQ(first_handle.instance().get(), second_handle.instance().get());
  EXPECT_TRUE(second_handle.instance()->IsInvoking());
  EXPECT_EQ(first_error.Get(), GlicInvokeError::kSuperseded);
  EXPECT_FALSE(second_error.IsReady());

  // Late teardown from the superseded caller must not cancel the replacement.
  first_handle.Cancel();
  EXPECT_TRUE(second_handle.instance()->IsInvoking());
  EXPECT_FALSE(second_error.IsReady());

  // Cancelling with the replacement's own handle succeeds.
  second_handle.Cancel();
  EXPECT_FALSE(second_handle.instance()->IsInvoking());
  EXPECT_EQ(second_error.Get(), GlicInvokeError::kCancelled);
}

TEST_F(GlicInvocationHandleTest, RejectedInvocationReturnsEmptyHandle) {
  base::test::TestFuture<GlicInvokeError> error_future;
  GlicInvocationHandle handle = StartHiddenClientInvoke(
      primary_tab_->tab, ConversationId{""},
      /*supersede_if_in_progress=*/false, error_future.GetCallback());
  EXPECT_FALSE(handle);
  EXPECT_FALSE(handle.instance());

  handle.Cancel();
  EXPECT_EQ(error_future.Get(), GlicInvokeError::kInvalidConversationId);
}

}  // namespace
}  // namespace glic
