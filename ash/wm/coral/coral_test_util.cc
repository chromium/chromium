// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_test_util.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/shell.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_chip_button.h"
#include "ash/wm/overview/birch/birch_chip_button_base.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "ash/wm/overview/birch/tab_app_selection_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

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

  BirchChipButton* coral_button = GetFirstCoralButton();
  event_generator->MoveMouseTo(
      coral_button->addon_view()->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  return coral_button->tab_app_selection_widget();
}

BirchChipButton* GetFirstCoralButton() {
  // Creating `OverviewGridTestApi` will crash if we aren't in overview mode.
  const std::vector<raw_ptr<BirchChipButtonBase>>& birch_chips =
      OverviewGridTestApi(Shell::GetPrimaryRootWindow()).GetBirchChips();
  CHECK_EQ(1u, birch_chips.size());

  auto* coral_button = views::AsViewClass<BirchChipButton>(birch_chips[0]);
  CHECK_EQ(BirchItemType::kCoral, coral_button->GetItem()->GetType());
  return coral_button;
}

}  // namespace ash
