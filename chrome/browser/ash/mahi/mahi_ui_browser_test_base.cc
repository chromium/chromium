// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_ui_browser_test_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/system/magic_boost/magic_boost_constants.h"
#include "ash/system/magic_boost/magic_boost_disclaimer_view.h"
#include "ash/test/ash_test_util.h"
#include "base/check_deref.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ash/mahi/mahi_test_util.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/signin/identity_manager_provider.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/manta/mahi_provider.h"
#include "components/manta/manta_service.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/provider_params.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// MockMahiProvider ------------------------------------------------------------

// A mock Mahi provider that returns predefined results asyncly.
class MockMahiProvider : public manta::MahiProvider {
 public:
  explicit MockMahiProvider(signin::IdentityManager* identity_manager)
      : manta::MahiProvider(
            /*url_loader_factory=*/nullptr,
            identity_manager,
            manta::ProviderParams()) {
    ON_CALL(*this, Summarize)
        .WillByDefault([](const std::string& input, const std::string&,
                          const std::optional<std::string>&,
                          const std::optional<std::string>&,
                          manta::MantaGenericCallback done_callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(done_callback),
                             base::DictValue().Set(/*key=*/"outputData",
                                                   GetMahiDefaultTestSummary()),
                             manta::MantaStatus{
                                 .status_code = manta::MantaStatusCode::kOk}));
        });

    ON_CALL(*this, QuestionAndAnswer)
        .WillByDefault(
            [](const std::string& content, const std::string&,
               const std::optional<std::string>&,
               const std::vector<manta::MahiProvider::MahiQAPair> QAHistory,
               const std::string& question,
               manta::MantaGenericCallback done_callback) {
              base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(
                      std::move(done_callback),
                      base::DictValue().Set(/*key=*/"outputData",
                                            GetMahiDefaultTestAnswer()),
                      manta::MantaStatus{.status_code =
                                             manta::MantaStatusCode::kOk}));
            });
  }

  // manta::MahiProvider:
  MOCK_METHOD(void,
              Summarize,
              (const std::string&,
               const std::string&,
               const std::optional<std::string>&,
               const std::optional<std::string>&,
               manta::MantaGenericCallback),
              (override));
  MOCK_METHOD(void,
              QuestionAndAnswer,
              (const std::string&,
               const std::string&,
               const std::optional<std::string>&,
               const std::vector<manta::MahiProvider::MahiQAPair>,
               const std::string&,
               manta::MantaGenericCallback),
              (override));
};

// MockMantaService ------------------------------------------------------------

// A mock manta service to inject a mock mahi provider.
class MockMantaService : public manta::MantaService {
 public:
  explicit MockMantaService(signin::IdentityManager* identity_manager)
      : manta::MantaService(
            /*shared_url_loader_factory=*/nullptr,
            identity_manager,
            /*is_demo_mode=*/false,
            /*is_otr_profile=*/false,
            /*chrome_version=*/"fake_version",
            /*chrome_channel=*/version_info::Channel::DEFAULT,
            /*locale=*/std::string()) {
    ON_CALL(*this, CreateMahiProvider).WillByDefault([identity_manager]() {
      return std::make_unique<MockMahiProvider>(identity_manager);
    });
  }

  // manta::MantaService:
  MOCK_METHOD(std::unique_ptr<manta::MahiProvider>,
              CreateMahiProvider,
              (),
              (override));
};

// Helpers ---------------------------------------------------------------------

signin::IdentityManager* GetIdentityManager(content::BrowserContext* context) {
  const AccountId* const account_id = ash::AnnotatedAccountId::Get(context);
  return account_id ? ash::IdentityManagerProvider::Get().Find(*account_id)
                    : nullptr;
}

std::unique_ptr<KeyedService> CreateMockMantaService(
    content::BrowserContext* context) {
  return std::make_unique<MockMantaService>(GetIdentityManager(context));
}

}  // namespace

MahiUiBrowserTestBase::MahiUiBrowserTestBase(bool apply_restrictions_override)
    : apply_restrictions_override_(apply_restrictions_override) {
  feature_list_.InitWithFeatures({chromeos::features::kFeatureManagementMahi},
                                 {});
}

MahiUiBrowserTestBase::~MahiUiBrowserTestBase() = default;

void MahiUiBrowserTestBase::SetUpCommandLine(base::CommandLine* command_line) {
  if (apply_restrictions_override_) {
    command_line->AppendSwitch(chromeos::switches::kMahiRestrictionsOverride);
  } else {
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "us");
  }

  InProcessBrowserTest::SetUpCommandLine(command_line);
}

void MahiUiBrowserTestBase::SetUpOnMainThread() {
  SystemWebAppBrowserTestBase::SetUpOnMainThread();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(Shell::GetPrimaryRootWindow());

  manta::MantaServiceFactory::GetInstance()->SetTestingFactory(
      browser()->GetProfile(), base::BindRepeating(&CreateMockMantaService));

  // Configure `https_server_` so that the test page is accessible.
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(https_server_.Start());

  // Navigate to the test page and wait until the page is ready.
  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("/mahi/test_article.html"));
  ASSERT_TRUE(render_frame_host);
  content::MainThreadFrameObserver(render_frame_host->GetRenderWidgetHost())
      .Wait();
}

void MahiUiBrowserTestBase::SetCanUseMantaService(std::optional<bool> can_use) {
  signin::IdentityManager* const identity_manager =
      GetIdentityManager(browser()->GetProfile());
  ASSERT_TRUE(identity_manager);

  // Ensure a primary account exists even when `can_use` is `std::nullopt`.
  AccountInfo account_info;
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    const CoreAccountInfo core_info =
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
    account_info = identity_manager->FindExtendedAccountInfo(core_info);
    if (account_info.IsEmpty()) {
      account_info =
          signin::MakeAccountAvailable(identity_manager, core_info.email);
    }
  } else {
    account_info = signin::MakePrimaryAccountAvailable(
        identity_manager, "test@example.com", signin::ConsentLevel::kSignin);
  }

  if (can_use.has_value()) {
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_manta_service(can_use.value());
    signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  }
}

void MahiUiBrowserTestBase::ClickDisclaimerViewButton(bool accept) {
  views::Widget* const disclaimer_view_widget =
      FindWidgetWithName(MagicBoostDisclaimerView::GetWidgetName());
  ASSERT_TRUE(disclaimer_view_widget);

  const views::View* const button =
      disclaimer_view_widget->GetContentsView()->GetViewByID(
          accept ? magic_boost::DisclaimerViewAcceptButton
                 : magic_boost::DisclaimerViewDeclineButton);
  ASSERT_TRUE(button);
  event_generator().MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();
}

void MahiUiBrowserTestBase::TypeStringToMahiMenuTextfield(
    views::Widget* mahi_menu_widget,
    const std::u16string& input) {
  ASSERT_TRUE(mahi_menu_widget);
  const views::View* const textfield =
      mahi_menu_widget->GetContentsView()->GetViewByID(
          chromeos::mahi::ViewID::kTextfield);
  ASSERT_TRUE(textfield);

  // Ensure focus on `textfield`.
  event_generator().MoveMouseTo(textfield->GetBoundsInScreen().CenterPoint());
  event_generator().ClickLeftButton();

  // Type `input_string`.
  for (char16_t c : input) {
    event_generator().PressAndReleaseKey(
        static_cast<ui::KeyboardCode>(ui::VKEY_A + c - u'a'));
  }
}

void MahiUiBrowserTestBase::WaitForSettingsToLoad() {
  // Wait until the Settings app finishes loading.
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  ash::SettingsAppManager::Get()->Open(
      CHECK_DEREF(ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
          browser()->GetProfile())),
      {});
  auto* const web_contents = waiter.Wait();
  ASSERT_TRUE(web_contents);
  content::WaitForLoadStop(web_contents);
}

}  // namespace ash
