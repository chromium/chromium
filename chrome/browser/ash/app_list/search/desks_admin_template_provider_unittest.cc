// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/desks_admin_template_provider.h"
#include <vector>

#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ash/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

namespace {

using testing::_;
using testing::Return;

class MockSavedDeskController : public ash::SavedDeskController {
 public:
  MockSavedDeskController() = default;

  MockSavedDeskController(const MockSavedDeskController&) = delete;
  MockSavedDeskController& operator=(const MockSavedDeskController&) = delete;

  ~MockSavedDeskController() override = default;

  MOCK_METHOD(std::vector<ash::AdminTemplateMetadata>,
              GetAdminTemplateMetadata,
              (),
              (const, override));
  MOCK_METHOD(bool,
              LaunchAdminTemplate,
              (const base::Uuid& template_uuid, int64_t default_display_id),
              (override));
};

}  // namespace

class DesksAdminTemplateProviderTest : public testing::Test {
 public:
  DesksAdminTemplateProviderTest() = default;
  ~DesksAdminTemplateProviderTest() override = default;

  void SetUp() override {
    search_controller_ = std::make_unique<TestSearchController>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("name");

    auto provider = std::make_unique<DesksAdminTemplateProvider>(
        profile_, &list_controller_);
    provider_ = provider.get();
    search_controller_->AddProvider(std::move(provider));

    Wait();
  }

  void TearDown() override {
    search_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  void StartZeroStateSearch() {
    search_controller_->StartZeroState(base::DoNothing(), base::TimeDelta());
  }

  const SearchProvider::Results& LastResults() {
    return search_controller_->last_results();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestSearchController> search_controller_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;
  ::test::TestAppListControllerDelegate list_controller_;
  raw_ptr<DesksAdminTemplateProvider, DanglingUntriaged> provider_ = nullptr;
};

// Tests that when there isn't a admin template, the results will be empty.
TEST_F(DesksAdminTemplateProviderTest, NoResultsWhenNoAdminTemplates) {
  MockSavedDeskController mock;
  std::vector<ash::AdminTemplateMetadata> empty_result = {};

  EXPECT_CALL(mock, GetAdminTemplateMetadata()).WillOnce(Return(empty_result));

  StartZeroStateSearch();
  Wait();

  EXPECT_TRUE(LastResults().empty());
}

// Tests that when there is a admin template, it will showing up on the bubble
// launcher in zero state. Also, when opening it, it will call the
// `LaunchAdminTemplate` function.
TEST_F(DesksAdminTemplateProviderTest, Basic) {
  MockSavedDeskController mock;

  std::vector<ash::AdminTemplateMetadata> results = {ash::AdminTemplateMetadata{
      .uuid = base::Uuid::GenerateRandomV4(), .name = u"test admin template"}};

  EXPECT_CALL(mock, GetAdminTemplateMetadata()).WillOnce(Return(results));
  EXPECT_CALL(mock, LaunchAdminTemplate(results[0].uuid, _))
      .WillOnce(Return(true));

  StartZeroStateSearch();
  Wait();

  ASSERT_EQ(1u, LastResults().size());

  ChromeSearchResult* result = LastResults().at(0).get();
  EXPECT_EQ(result->title(), u"test admin template");
  result->Open(/*event_flags=*/0);
}

}  // namespace app_list::test
