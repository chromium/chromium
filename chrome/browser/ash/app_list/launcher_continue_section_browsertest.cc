// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/test/app_list_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/files/file_path.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/drive_integration_service_browser_test_base.h"
#include "chrome/browser/ash/drive/drivefs_test_support.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/ash/file_suggest/file_suggest_test_util.h"
#include "chrome/browser/ash/file_suggest/local_file_suggestion_provider.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/ash/components/drivefs/fake_drivefs.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"

using ::testing::_;
using ::testing::Field;
using ::testing::Pointee;

namespace {

struct QueryItemInfo {
  base::FilePath path;

  base::Time last_modified_time;
  std::optional<base::Time> modified_by_me_time;
  std::optional<std::string> last_modifying_user;

  base::Time last_viewed_by_me_time;

  std::optional<base::Time> shared_with_me_time;
  std::optional<std::string> sharing_user;
};

std::vector<drivefs::mojom::QueryItemPtr> CreateQueryItems(
    const std::vector<QueryItemInfo>& items) {
  std::vector<drivefs::mojom::QueryItemPtr> results;
  for (const auto& item : items) {
    auto result = drivefs::mojom::QueryItem::New();
    result->path = item.path;
    result->metadata = drivefs::mojom::FileMetadata::New();
    result->metadata->modification_time = item.last_modified_time;
    result->metadata->modified_by_me_time = item.modified_by_me_time;
    result->metadata->last_viewed_by_me_time = item.last_viewed_by_me_time;
    if (item.last_modifying_user) {
      result->metadata->last_modifying_user = drivefs::mojom::UserInfo::New();
      result->metadata->last_modifying_user->display_name =
          *item.last_modifying_user;
    }
    result->metadata->shared_with_me_time = item.shared_with_me_time;
    if (item.sharing_user) {
      result->metadata->sharing_user = drivefs::mojom::UserInfo::New();
      result->metadata->sharing_user->display_name = *item.sharing_user;
    }
    result->metadata->capabilities = drivefs::mojom::Capabilities::New();
    results.push_back(std::move(result));
  }
  return results;
}

class FakeSearchQuery : public drivefs::mojom::SearchQuery {
 public:
  FakeSearchQuery() = default;
  explicit FakeSearchQuery(std::vector<drivefs::mojom::QueryItemPtr> results)
      : results_(std::move(results)) {}

  FakeSearchQuery(const FakeSearchQuery&) = delete;
  FakeSearchQuery& operator=(const FakeSearchQuery&) = delete;
  ~FakeSearchQuery() override = default;

  void GetNextPage(GetNextPageCallback callback) override {
    if (next_page_called_) {
      std::move(callback).Run(drive::FILE_ERROR_OK, {});
      return;
    }
    next_page_called_ = true;
    std::move(callback).Run(drive::FILE_ERROR_OK, std::move(results_));
  }

 private:
  std::vector<drivefs::mojom::QueryItemPtr> results_;
  bool next_page_called_ = false;
};

}  // namespace

class LauncherContinueSectionTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  LauncherContinueSectionTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ash::features::kLauncherContinueSectionWithRecentsRollout,
          {{"mix_local_and_drive",
            MixLocalAndDriveFiles() ? "true" : "false"}}},
         {ash::features::kShowSharingUserInLauncherContinueSection, {}}},
        {});
  }
  ~LauncherContinueSectionTest() override = default;
  LauncherContinueSectionTest(const LauncherContinueSectionTest&) = delete;
  LauncherContinueSectionTest& operator=(const LauncherContinueSectionTest&) =
      delete;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override {
    create_drive_integration_service_ = base::BindRepeating(
        &LauncherContinueSectionTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
  }

  void SetUpOnMainThread() override {
    AppListClientImpl::GetInstance()->UpdateProfile();

    ash::AppListTestApi test_api;
    test_api.DisableAppListNudge(true);
    test_api.SetContinueSectionPrivacyNoticeAccepted();

    ash::ShellTestApi().SetTabletModeEnabledForTest(IsTabletMode());

    Profile* const profile = browser()->profile();

    ash::SystemWebAppManager::GetForTest(profile)
        ->InstallSystemAppsForTesting();

    ash::WaitUntilFileSuggestServiceReady(
        ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
            profile));
  }

  bool IsTabletMode() const { return std::get<0>(GetParam()); }

  bool MixLocalAndDriveFiles() const { return std::get<1>(GetParam()); }

  base::FilePath AddTestLocalFile(const std::string& file_name,
                                  const base::Time& last_access_time,
                                  const base::Time& last_modified_time) {
    const base::FilePath mount_path =
        file_manager::util::GetDownloadsFolderForProfile(browser()->profile());
    base::FilePath absolute_path = mount_path.AppendASCII(file_name);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::WriteFile(absolute_path, file_name));
      EXPECT_TRUE(
          base::TouchFile(absolute_path, last_access_time, last_modified_time));
    }

    // Notify local file suggestion provider that the file has changed, so it
    // gets picked up as a suggestion candidate.
    using FileOpenType = file_manager::file_tasks::FileTasksObserver::OpenType;
    using FileOpenEvent =
        file_manager::file_tasks::FileTasksObserver::FileOpenEvent;
    FileOpenEvent e;
    e.path = absolute_path;
    e.open_type = FileOpenType::kOpen;
    std::vector<FileOpenEvent> open_events;
    open_events.push_back(std::move(e));

    ash::FileSuggestKeyedServiceFactory::GetInstance()
        ->GetService(browser()->profile())
        ->local_file_suggestion_provider_for_test()
        ->OnFilesOpened(open_events);

    return absolute_path;
  }

  base::FilePath AddTestDriveFile(const std::string& file_name,
                                  const std::string& alternate_url) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    drive::DriveIntegrationService* drive_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(
            browser()->profile());
    EXPECT_TRUE(drive_service->IsMounted());
    base::FilePath mount_path = drive_service->GetMountPointPath();

    base::FilePath absolute_path = mount_path.AppendASCII(file_name);
    EXPECT_TRUE(base::WriteFile(absolute_path, file_name));
    base::FilePath relative_path;
    EXPECT_TRUE(
        drive_service->GetRelativeDrivePath(absolute_path, &relative_path));

    drivefs::FakeMetadata metadata;
    metadata.path = relative_path;
    metadata.alternate_url = alternate_url;

    drivefs::FakeDriveFs* drive_fs =
        GetFakeDriveFsForProfile(browser()->profile());
    drive_fs->SetMetadata(std::move(metadata));

    std::vector<drivefs::mojom::FileChangePtr> changes;
    changes.emplace_back(std::in_place, relative_path,
                         drivefs::mojom::FileChange::Type::kCreate);
    drive_fs->delegate()->OnFilesChanged(mojo::Clone(changes));
    drive_fs->delegate().FlushForTesting();

    return relative_path;
  }

  drivefs::FakeDriveFs* GetFakeDriveFsForProfile(Profile* profile) {
    return &fake_drivefs_helpers_[profile]->fake_drivefs();
  }

  void ShowAppListAndWaitForZeroStateResults() {
    app_list::SearchResultsChangedWaiter results_changed_waiter(
        AppListClientImpl::GetInstance()->search_controller(),
        {ash::AppListSearchResultType::kZeroStateFile,
         ash::AppListSearchResultType::kZeroStateDrive});

    ash::AcceleratorController::Get()->PerformActionIfEnabled(
        ash::AcceleratorAction::kToggleAppList, {});
    if (IsTabletMode()) {
      ash::AppListTestApi().WaitForAppListShowAnimation(
          /*is_bubble_window=*/false);
    } else {
      ash::AppListTestApi().WaitForBubbleWindow(
          /*wait_for_opening_animation=*/true);
    }

    results_changed_waiter.Wait();

    // Continue section content gets updated asynchronously on UI thread, make
    // sure that the task to update the continue section runs.
    base::RunLoop flush;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, flush.QuitClosure());
    flush.Run();
  }

  std::vector<std::u16string> GetContinueTaskTitles(
      const std::vector<ash::ContinueTaskView*>& views) {
    std::vector<std::u16string> titles;
    for (const auto* const view : views) {
      titles.push_back(view->result()->title());
    }
    return titles;
  }

  std::vector<std::u16string> GetContinueTaskDescriptions(
      const std::vector<ash::ContinueTaskView*>& views) {
    std::vector<std::u16string> descriptions;
    for (const auto* const view : views) {
      descriptions.push_back(view->result()->details());
    }
    return descriptions;
  }

  void ClickOnView(views::View* target_view) {
    ui::test::EventGenerator event_generator(
        target_view->GetWidget()->GetNativeWindow()->GetRootWindow());
    target_view->GetWidget()->LayoutRootViewIfNecessary();
    event_generator.MoveMouseTo(target_view->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

  static base::Time GetReferenceTime() {
    base::Time time;
    EXPECT_TRUE(base::Time::FromString("Wed, 28 Feb 2023 11:00:00 UTC", &time));
    return time;
  }

 private:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath mount_path = profile->GetPath().Append("drivefs");
    fake_drivefs_helpers_[profile] =
        std::make_unique<drive::FakeDriveFsHelper>(profile, mount_path);
    auto* integration_service = new drive::DriveIntegrationService(
        profile, std::string(), mount_path,
        fake_drivefs_helpers_[profile]->CreateFakeDriveFsListenerFactory());
    return integration_service;
  }

  base::subtle::ScopedTimeClockOverrides time_override_{
      &LauncherContinueSectionTest::GetReferenceTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr};

  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  std::map<Profile*, std::unique_ptr<drive::FakeDriveFsHelper>>
      fake_drivefs_helpers_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LauncherContinueSectionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(LauncherContinueSectionTest, ShowDriveFiles) {
  base::FilePath file_1 =
      AddTestDriveFile("Test File 1.gdoc", "http://fake/test_file_1");
  base::FilePath file_2 =
      AddTestDriveFile("Test File 2.gdoc", "http://fake/test_file_2");
  base::FilePath file_3 =
      AddTestDriveFile("Test File 3.gdoc", "http://fake/test_file_3");
  base::FilePath file_4 =
      AddTestDriveFile("Test File 4.gdoc", "http://fake/test_file_4");

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_1,
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .modified_by_me_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_1,
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .modified_by_me_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(2)},
             {.path = file_2,
              .last_modified_time = GetReferenceTime() - base::Days(5),
              .modified_by_me_time = GetReferenceTime() - base::Days(6),
              .last_modifying_user = "Test User 2"},
             {.path = file_4,
              .last_modified_time = GetReferenceTime() - base::Days(5),
              .last_modifying_user = "Test User 3"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_3,
              .last_modified_time = GetReferenceTime() - base::Minutes(30),
              .shared_with_me_time = GetReferenceTime() - base::Hours(1),
              .sharing_user = "Test User 3"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });

  ShowAppListAndWaitForZeroStateResults();

  std::vector<ash::ContinueTaskView*> continue_tasks =
      ash::AppListTestApi().GetContinueTaskViews();
  EXPECT_EQ(3u, continue_tasks.size());
  std::vector<std::u16string> expected_titles = {u"Test File 1", u"Test File 2",
                                                 u"Test File 3"};
  EXPECT_EQ(expected_titles, GetContinueTaskTitles(continue_tasks));
  std::vector<std::u16string> expected_descriptions = {
      u"You opened · Feb 26", u"Test User 2 edited · Feb 23",
      u"Test User 3 shared · 10:00 AM"};
  EXPECT_EQ(expected_descriptions, GetContinueTaskDescriptions(continue_tasks));

  ASSERT_GT(continue_tasks.size(), 1u);

  content::TestNavigationObserver navigation_observer(
      GURL("http://fake/test_file_1"));

  ClickOnView(continue_tasks[0]);

  navigation_observer.StartWatchingNewWebContents();
  navigation_observer.Wait();
}

IN_PROC_BROWSER_TEST_P(LauncherContinueSectionTest, ShowDriveAndLocalFiles) {
  base::FilePath file_1 =
      AddTestDriveFile("Test File 1.gdoc", "http://fake/test_file_1");
  base::FilePath file_2 =
      AddTestDriveFile("Test File 2.gdoc", "http://fake/test_file_2");
  base::FilePath file_3 =
      AddTestDriveFile("Test File 3.gdoc", "http://fake/test_file_3");

  base::FilePath local_file = AddTestLocalFile(
      "Test Local File.txt", GetReferenceTime() - base::Days(4),
      GetReferenceTime() - base::Days(5));

  auto* fake_drivefs = GetFakeDriveFsForProfile(browser()->profile());
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kLastViewedByMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_1,
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .modified_by_me_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time =
                  GetReferenceTime() - base::Minutes(2)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kLastModified))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_1,
              .last_modified_time = GetReferenceTime() - base::Days(4),
              .modified_by_me_time = GetReferenceTime() - base::Days(4),
              .last_viewed_by_me_time = GetReferenceTime() - base::Minutes(2)},
             {.path = file_2,
              .last_modified_time = GetReferenceTime(),
              .modified_by_me_time = GetReferenceTime() - base::Days(6),
              .last_modifying_user = "Test User 2",
              .last_viewed_by_me_time = GetReferenceTime() - base::Days(7)}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });
  EXPECT_CALL(
      *fake_drivefs,
      StartSearchQuery(
          _, Pointee(Field(
                 &drivefs::mojom::QueryParameters::sort_field,
                 drivefs::mojom::QueryParameters::SortField::kSharedWithMe))))
      .WillOnce([&](mojo::PendingReceiver<drivefs::mojom::SearchQuery>
                        pending_receiver,
                    drivefs::mojom::QueryParametersPtr query_params) {
        auto search_query = std::make_unique<FakeSearchQuery>(CreateQueryItems(
            {{.path = file_3,
              .last_modified_time = GetReferenceTime() - base::Minutes(30),
              .shared_with_me_time = GetReferenceTime() - base::Hours(1),
              .sharing_user = "Test User 3"}}));
        mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                    std::move(pending_receiver));
      });

  ShowAppListAndWaitForZeroStateResults();

  std::vector<ash::ContinueTaskView*> continue_tasks =
      ash::AppListTestApi().GetContinueTaskViews();
  EXPECT_EQ(4u, continue_tasks.size());

  std::vector<std::u16string> expected_titles;
  if (MixLocalAndDriveFiles()) {
    expected_titles = {u"Test File 1", u"Test Local File.txt", u"Test File 2",
                       u"Test File 3"};
  } else {
    expected_titles = {u"Test File 1", u"Test File 2", u"Test File 3",
                       u"Test Local File.txt"};
  }
  EXPECT_EQ(expected_titles, GetContinueTaskTitles(continue_tasks));

  std::vector<std::u16string> expected_descriptions;
  if (MixLocalAndDriveFiles()) {
    expected_descriptions = {u"You opened · just now", u"You opened · Feb 24",
                             u"Test User 2 edited · just now",
                             u"Test User 3 shared · 10:00 AM"};
  } else {
    expected_descriptions = {
        u"You opened · just now", u"Test User 2 edited · just now",
        u"Test User 3 shared · 10:00 AM", u"You opened · Feb 24"};
  }
  EXPECT_EQ(expected_descriptions, GetContinueTaskDescriptions(continue_tasks));

  ASSERT_GT(continue_tasks.size(), 1u);

  content::TestNavigationObserver navigation_observer(
      GURL("http://fake/test_file_1"));

  ClickOnView(continue_tasks[0]);

  navigation_observer.StartWatchingNewWebContents();
  navigation_observer.Wait();
}
