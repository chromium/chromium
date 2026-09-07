// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_test_util.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/coral_chip_button.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "chromeos/ash/components/signin/fake_identity_manager_provider.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

class ScopedCoralGenAIAvailability::Impl {
 public:
  explicit Impl(const AccountId& account_id)
      : identity_test_env_(std::make_unique<signin::IdentityTestEnvironment>()),
        provider_(std::make_unique<FakeIdentityManagerProvider>()) {
    // Sign `account_id` in as the primary user, so
    // BirchCoralProvider::CheckGenAIAgeAvailability() resolves the active
    // account to it. The UserManager and SessionManager are installed by
    // AshTestHelper.
    auto* const session_manager = session_manager::SessionManager::Get();
    // The signed-in account is the primary user; there must be no session yet.
    CHECK(session_manager->sessions().empty());
    CHECK(user_manager::TestHelper(user_manager::UserManager::Get())
              .AddRegularUser(account_id));
    session_manager->CreateSession(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
        /*new_user=*/false, /*has_active_session=*/false);
    session_manager->SessionStarted();

    // Make `account_id`'s IdentityManager report the ChromeOS GenAI capability,
    // and register it so CheckGenAIAgeAvailability() resolves to "available".
    // MakePrimaryAccountAvailable blocks until refresh tokens are loaded, so
    // the age check completes synchronously.
    AccountInfo account = identity_test_env_->MakePrimaryAccountAvailable(
        account_id.GetUserEmail(), signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account);
    mutator.set_can_use_chromeos_generative_ai(true);
    identity_test_env_->UpdateAccountInfoForAccount(account);
    provider_->SetIdentityManagerForAccount(
        account_id, identity_test_env_->identity_manager());
  }

  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  ~Impl() = default;

 private:
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<FakeIdentityManagerProvider> provider_;
};

ScopedCoralGenAIAvailability::ScopedCoralGenAIAvailability(
    const AccountId& account_id)
    : impl_(std::make_unique<Impl>(account_id)) {}

ScopedCoralGenAIAvailability::~ScopedCoralGenAIAvailability() = default;

TestEntity::TestEntity(const std::string& title,
                       const std::variant<GURL, std::string>& id)
    : title(title), id(id) {}

TestEntity::TestEntity(const TestEntity&) = default;

TestEntity& TestEntity::operator=(const TestEntity&) = default;

TestEntity::~TestEntity() = default;

coral::mojom::GroupPtr CreateTestGroup(const std::vector<TestEntity>& entities,
                                       const std::optional<std::string>& title,
                                       const base::Token& id) {
  auto test_group = coral::mojom::Group::New();
  test_group->id = id;
  test_group->title = title;

  for (const TestEntity& entity : entities) {
    if (std::holds_alternative<GURL>(entity.id)) {
      test_group->entities.push_back(coral::mojom::Entity::NewTab(
          coral::mojom::Tab::New(entity.title, std::get<GURL>(entity.id))));
    } else if (std::holds_alternative<std::string>(entity.id)) {
      test_group->entities.push_back(
          coral::mojom::Entity::NewApp(coral::mojom::App::New(
              entity.title, std::get<std::string>(entity.id))));
    } else {
      NOTREACHED();
    }
  }

  return test_group;
}

coral::mojom::GroupPtr CreateDefaultTestGroup() {
  return CreateTestGroup({{"Reddit", GURL("https://www.reddit.com/")},
                          {"Figma", GURL("https://www.figma.com/")},
                          {"Notion", GURL("https://www.notion.so/")},
                          {"Settings", "odknhmnlageboeamepcngndbggdpaobj"},
                          {"Files", "fkiggjmkendpmbegkagpmagjepfkpmeb"}},
                         "Coral Group");
}

void OverrideTestResponse(std::vector<coral::mojom::GroupPtr> test_groups,
                          CoralSource source) {
  auto test_response = std::make_unique<CoralResponse>();
  test_response->set_source(source);
  test_response->set_groups(std::move(test_groups));
  BirchCoralProvider::Get()->OverrideCoralResponseForTest(
      std::move(test_response));
}

TabAppSelectionHost* ShowAndGetSelectorMenu(
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kTests);

  const std::vector<raw_ptr<BirchChipButtonBase>>& birch_chips =
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips();
  CHECK_EQ(1u, birch_chips.size());

  CoralChipButton* coral_button = GetFirstCoralButton();
  event_generator->MoveMouseTo(
      coral_button->addon_view()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  return coral_button->tab_app_selection_widget();
}

CoralChipButton* GetFirstCoralButton() {
  // Creating `OverviewGridTestApi` will crash if we aren't in overview mode.
  const std::vector<raw_ptr<BirchChipButtonBase>>& birch_chips =
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips();
  CHECK_GE(birch_chips.size(), 1u);

  auto* coral_button = views::AsViewClass<CoralChipButton>(birch_chips[0]);
  CHECK(!!coral_button);
  return coral_button;
}

size_t GetCoralButtonNum() {
  return std::ranges::count_if(
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips(),
      [](auto& chip) {
        return chip->GetItem()->GetType() == BirchItemType::kCoral;
      });
}

}  // namespace ash
