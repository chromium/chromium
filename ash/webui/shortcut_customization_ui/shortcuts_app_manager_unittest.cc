// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_configuration.h"
#include "ash/public/mojom/accelerator_info.mojom-shared.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/webui/shortcut_customization_ui/backend/accelerator_configuration_provider.h"
#include "ash/webui/shortcut_customization_ui/backend/search/fake_search_data.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept_registry.h"
#include "ash/webui/shortcut_customization_ui/shortcuts_app_manager.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash::shortcut_ui {

class ShortcutsAppManagerTest : public AshTestBase {
 protected:
  ShortcutsAppManagerTest() = default;
  ~ShortcutsAppManagerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();

    local_search_service_proxy_ =
        std::make_unique<local_search_service::LocalSearchServiceProxy>(
            /*for_testing=*/true);

    manager_ = std::make_unique<ShortcutsAppManager>(
        local_search_service_proxy_.get(),
        Shell::Get()->session_controller()->GetActivePrefService());

    // Let the RunLoop so that the AshAcceleratorConfiguration can load its
    // default accelerators.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

  void ValidateSearchConceptById(
      const base::flat_map<std::string, SearchConcept>& search_concepts_map,
      const std::string search_concept_id,
      const mojom::AcceleratorSource expected_source,
      const uint32_t expected_action) const {
    EXPECT_TRUE(search_concepts_map.contains(search_concept_id));
    EXPECT_EQ(search_concepts_map.at(search_concept_id)
                  .accelerator_layout_info->source,
              expected_source);
    EXPECT_EQ(search_concepts_map.at(search_concept_id)
                  .accelerator_layout_info->action,
              expected_action);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<ShortcutsAppManager> manager_;
};

TEST_F(ShortcutsAppManagerTest, SetSearchConcepts) {
  // Create all the fake AcceleratorInfo maps for use in the fake config.
  AcceleratorConfigurationProvider::ActionIdToAcceleratorsInfoMap ash_info_map;
  ash_info_map.insert({fake_search_data::FakeActionIds::kAction1,
                       fake_search_data::CreateFakeAcceleratorInfoList()});
  ash_info_map.insert({fake_search_data::FakeActionIds::kAction2,
                       fake_search_data::CreateFakeAcceleratorInfoList(
                           ash::mojom::AcceleratorState::kDisabledByUser)});

  AcceleratorConfigurationProvider::ActionIdToAcceleratorsInfoMap
      browser_info_map;
  browser_info_map.insert({fake_search_data::FakeActionIds::kAction3,
                           fake_search_data::CreateFakeAcceleratorInfoList()});
  browser_info_map.insert({fake_search_data::FakeActionIds::kAction4,
                           fake_search_data::CreateFakeAcceleratorInfoList()});

  // Create the fake config.
  shortcut_ui::AcceleratorConfigurationProvider::AcceleratorConfigurationMap
      fake_config;
  fake_config.insert({mojom::AcceleratorSource::kAsh, std::move(ash_info_map)});
  fake_config.insert(
      {mojom::AcceleratorSource::kBrowser, std::move(browser_info_map)});

  // Create the fake AcceleratorLayoutInfos list.
  std::vector<mojom::AcceleratorLayoutInfoPtr> fake_layout_infos;
  fake_layout_infos.push_back(fake_search_data::CreateFakeAcceleratorLayoutInfo(
      u"Open launcher", ash::mojom::AcceleratorSource::kAsh,
      fake_search_data::FakeActionIds::kAction1,
      ash::mojom::AcceleratorLayoutStyle::kDefault));
  fake_layout_infos.push_back(fake_search_data::CreateFakeAcceleratorLayoutInfo(
      u"Open/close calendar", ash::mojom::AcceleratorSource::kAsh,
      fake_search_data::FakeActionIds::kAction2,
      ash::mojom::AcceleratorLayoutStyle::kDefault));
  fake_layout_infos.push_back(fake_search_data::CreateFakeAcceleratorLayoutInfo(
      u"Open new tab", ash::mojom::AcceleratorSource::kBrowser,
      fake_search_data::FakeActionIds::kAction3,
      ash::mojom::AcceleratorLayoutStyle::kDefault));
  fake_layout_infos.push_back(fake_search_data::CreateFakeAcceleratorLayoutInfo(
      u"Close tab", ash::mojom::AcceleratorSource::kBrowser,
      fake_search_data::FakeActionIds::kAction4,
      ash::mojom::AcceleratorLayoutStyle::kDefault));

  auto& registry_search_concepts =
      manager_->search_concept_registry_.get()->result_id_to_search_concept_;

  // AshAcceleratorConfiguration loads some initial accelerators (which ends up
  // populating the registry map), so clear the registry map to get a fresh
  // slate for the test.
  registry_search_concepts.clear();
  EXPECT_EQ(registry_search_concepts.size(), 0u);

  manager_->SetSearchConcepts(std::move(fake_config),
                              std::move(fake_layout_infos));
  base::RunLoop().RunUntilIdle();

  // Disabled accelerator info is included, which will be displayed as 'No
  // shortcut assigned' in the frontend.
  EXPECT_EQ(registry_search_concepts.size(), 4u);

  // Test that the expected search concepts are present and check a few
  // attributes to be sure.
  ValidateSearchConceptById(/*search_concepts_map=*/registry_search_concepts,
                            /*search_concept_id=*/"0-1",
                            /*expected_source=*/mojom::AcceleratorSource::kAsh,
                            /*expected_action=*/fake_search_data::kAction1);
  ValidateSearchConceptById(
      /*search_concepts_map=*/registry_search_concepts,
      /*search_concept_id=*/"0-2",
      /*expected_source=*/mojom::AcceleratorSource::kAsh,
      /*expected_action=*/fake_search_data::kAction2);
  ValidateSearchConceptById(
      /*search_concepts_map=*/registry_search_concepts,
      /*search_concept_id=*/"2-3",
      /*expected_source=*/mojom::AcceleratorSource::kBrowser,
      /*expected_action=*/fake_search_data::kAction3);
  ValidateSearchConceptById(
      /*search_concepts_map=*/registry_search_concepts,
      /*search_concept_id=*/"2-4",
      /*expected_source=*/mojom::AcceleratorSource::kBrowser,
      /*expected_action=*/fake_search_data::kAction4);
}

}  // namespace ash::shortcut_ui
