// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_test_base.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace contextual_tasks {

FakeContextualTasksEligibilityManager::FakeContextualTasksEligibilityManager(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    AimEligibilityService* aim_eligibility_service)
    : ContextualTasksEligibilityManager(pref_service,
                                        identity_manager,
                                        aim_eligibility_service) {
  MaybeNotifyEligibilityChanged();
}

FakeContextualTasksEligibilityManager::
    ~FakeContextualTasksEligibilityManager() = default;

void FakeContextualTasksEligibilityManager::SetIsEligible(bool eligible) {
  is_eligible_ = eligible;
  MaybeNotifyEligibilityChanged();
}

bool FakeContextualTasksEligibilityManager::IsEligibleWithoutIdentity() const {
  return is_eligible_;
}

bool FakeContextualTasksEligibilityManager::CalculateEligibility() const {
  return is_eligible_;
}

MockUiServiceForUrlIntercept::MockUiServiceForUrlIntercept(
    Profile* profile,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    AimEligibilityService* aim_eligibility_service,
    signin::IdentityManager* identity_manager)
    : ContextualTasksUiService(
          profile,
          std::make_unique<NiceMock<MockContextualTasksUiServiceDelegate>>(),
          contextual_tasks_service,
          identity_manager,
          aim_eligibility_service,
          std::make_unique<FakeContextualTasksEligibilityManager>(
              profile->GetPrefs(),
              identity_manager,
              aim_eligibility_service),
          /*cookie_synchronizer=*/nullptr) {}

MockUiServiceForUrlIntercept::~MockUiServiceForUrlIntercept() = default;

FakeContextualTasksEligibilityManager*
MockUiServiceForUrlIntercept::GetFakeEligibilityManager() {
  return static_cast<FakeContextualTasksEligibilityManager*>(
      GetEligibilityManager());
}

bool MockUiServiceForUrlIntercept::HandleNavigationImpl(
    content::OpenURLParams url_params,
    content::WebContents* source_contents,
    tabs::TabInterface* tab,
    bool is_from_embedded_page,
    bool from_can_create_window,
    bool is_same_site_or_from_ui,
    bool is_mobile_ua,
    const std::optional<url::Origin>& initiator_origin,
    const std::optional<content::GlobalRenderFrameHostToken>&
        initiator_frame_token,
    const blink::mojom::WindowFeatures& window_features) {
  return ContextualTasksUiService::HandleNavigationImpl(
      std::move(url_params), source_contents, tab, is_from_embedded_page,
      from_can_create_window, is_same_site_or_from_ui, is_mobile_ua,
      initiator_origin, initiator_frame_token, window_features);
}

ContextualTasksUiServiceTestBase::ContextualTasksUiServiceTestBase(
    base::test::TaskEnvironment::TimeSource time_source)
    : content::RenderViewHostTestHarness(time_source) {}

ContextualTasksUiServiceTestBase::~ContextualTasksUiServiceTestBase() = default;

void ContextualTasksUiServiceTestBase::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  // IdentityTestEnvironment must be created after the TaskEnvironment.
  identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();

  profile_ = std::make_unique<TestingProfile>();
  contextual_tasks_service_ = std::make_unique<MockContextualTasksService>();
  aim_eligibility_service_ = std::make_unique<MockAimEligibilityService>(
      prefs_, nullptr, nullptr, nullptr);

  // By default, assume URLs have the correct URL params to be intercepted.
  ON_CALL(*aim_eligibility_service_, HasAimUrlParams(_))
      .WillByDefault(Return(true));
  ON_CALL(*aim_eligibility_service_, IsCobrowseEligible())
      .WillByDefault(Return(true));
  ON_CALL(*aim_eligibility_service_, RegisterEligibilityChangedCallback(_))
      .WillByDefault([](base::RepeatingClosure) {
        return base::CallbackListSubscription();
      });

  service_for_nav_ = static_cast<MockUiServiceForUrlIntercept*>(
      ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(),
          base::BindRepeating(
              [](ContextualTasksService* cts, AimEligibilityService* aes,
                 signin::IdentityManager* im, content::BrowserContext* context)
                  -> std::unique_ptr<KeyedService> {
                Profile* profile = Profile::FromBrowserContext(context);
                auto mock_service =
                    std::make_unique<MockUiServiceForUrlIntercept>(profile, cts,
                                                                   aes, im);
                ON_CALL(*mock_service, IsUrlForPrimaryAccount(_))
                    .WillByDefault(Return(true));
                ON_CALL(*mock_service,
                        IsSignedInToBrowserWithValidCredentials())
                    .WillByDefault(Return(true));
                return mock_service;
              },
              contextual_tasks_service_.get(), aim_eligibility_service_.get(),
              identity_test_env_->identity_manager())));

  // Create a real service for testing non-mocked methods like GetAccessToken.
  // We pass the IdentityManager from the test environment.
  real_service_ = std::make_unique<ContextualTasksUiService>(
      profile_.get(),
      std::make_unique<NiceMock<MockContextualTasksUiServiceDelegate>>(),
      contextual_tasks_service_.get(), identity_test_env_->identity_manager(),
      aim_eligibility_service_.get(),
      std::make_unique<ContextualTasksEligibilityManager>(
          profile_->GetPrefs(), identity_test_env_->identity_manager(),
          aim_eligibility_service_.get()),
      /*cookie_synchronizer=*/nullptr);

  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  // Set up default search provider.
  TemplateURLData data;
  data.SetShortName(u"TestEngine");
  data.SetKeyword(u"TestEngine");
  data.SetURL("https://www.google.com/search?q={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Ensure template url service is fully loaded before executing any test
  // logic.
  if (!template_url_service->loaded()) {
    base::test::TestFuture<bool> loaded_future;
    base::CallbackListSubscription subscription =
        template_url_service->RegisterOnLoadedCallback(base::BindOnce(
            [](base::test::TestFuture<bool>* future) {
              future->SetValue(true);
            },
            &loaded_future));
    template_url_service->Load();
    ASSERT_TRUE(loaded_future.Get());
  }
}

void ContextualTasksUiServiceTestBase::TearDown() {
  real_service_ = nullptr;
  service_for_nav_ = nullptr;
  ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
      profile_.get(), ContextualTasksUiServiceFactory::TestingFactory());
  contextual_tasks_service_ = nullptr;
  identity_test_env_.reset();
  profile_ = nullptr;
  content::RenderViewHostTestHarness::TearDown();
}

std::unique_ptr<content::BrowserContext>
ContextualTasksUiServiceTestBase::CreateBrowserContext() {
  return std::make_unique<TestingProfile>();
}

}  // namespace contextual_tasks
