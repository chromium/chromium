// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/default_browser_monitor.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/win/registry_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace default_browser {

namespace {

constexpr wchar_t kHttpAssociationPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http";
constexpr wchar_t kHttpsAssociationPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations"
    L"\\https";
constexpr wchar_t kHttpUserChoicePath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http"
    L"\\UserChoice";
constexpr wchar_t kHttpUserChoiceLatestProgIdPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http"
    L"\\UserChoiceLatest\\ProgId";
constexpr wchar_t kHttpsUserChoiceLatestProgIdPath[] =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations"
    L"\\https\\UserChoiceLatest\\ProgId";

void CreateKey(const wchar_t* path) {
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, path,
                                      KEY_WRITE | KEY_CREATE_SUB_KEY));
}

void CreateAndWriteProgId(const wchar_t* path, const wchar_t* prog_id) {
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Create(HKEY_CURRENT_USER, path,
                                      KEY_WRITE | KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProgId", prog_id));
}

void WriteProgIdToExistingKey(const wchar_t* path, const wchar_t* prog_id) {
  base::win::RegKey key;
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, path, KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"ProgId", prog_id));
}

}  // namespace

class DefaultBrowserMonitorWinTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
  }

  void InitFeatureAndTaskEnvironment(bool enable_feature) {
    if (enable_feature) {
      feature_list_.InitAndEnableFeatureWithParameters(
          kDefaultBrowserMonitorUrlAssociations,
          {{kMonitorDebounceDelay.name, "0ms"}});
    } else {
      feature_list_.InitAndDisableFeature(
          kDefaultBrowserMonitorUrlAssociations);
    }
    task_environment_.emplace();
  }

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
  base::test::ScopedFeatureList feature_list_;
  std::optional<base::test::TaskEnvironment> task_environment_;
};

TEST_F(DefaultBrowserMonitorWinTest,
       DetectsUserChoiceLatestWriteWhenFeatureEnabled) {
  InitFeatureAndTaskEnvironment(/*enable_feature=*/true);

  // Create only the parent UrlAssociations\http key (no legacy UserChoice).
  CreateKey(kHttpAssociationPath);

  DefaultBrowserMonitor monitor;
  base::RunLoop run_loop;
  bool notified = false;
  auto subscription = monitor.RegisterDefaultBrowserChanged(base::BindRepeating(
      [](bool* flag, base::RunLoop* loop) {
        *flag = true;
        loop->Quit();
      },
      &notified, &run_loop));
  monitor.StartMonitor();

  CreateAndWriteProgId(kHttpUserChoiceLatestProgIdPath, L"OtherBrowserHTML");

  run_loop.Run();
  EXPECT_TRUE(notified);
}

TEST_F(DefaultBrowserMonitorWinTest,
       IgnoresUserChoiceLatestWriteWhenFeatureDisabled) {
  InitFeatureAndTaskEnvironment(/*enable_feature=*/false);

  CreateKey(kHttpUserChoicePath);
  CreateKey(kHttpUserChoiceLatestProgIdPath);

  DefaultBrowserMonitor monitor;
  base::RunLoop run_loop;
  int call_count = 0;
  auto subscription = monitor.RegisterDefaultBrowserChanged(base::BindRepeating(
      [](int* count, base::RunLoop* loop) {
        ++(*count);
        loop->Quit();
      },
      &call_count, &run_loop));
  monitor.StartMonitor();

  // Writing to UserChoiceLatest should not notify when the feature is disabled,
  // while a subsequent write to legacy UserChoice flushes the watcher and
  // triggers the single expected notification.
  WriteProgIdToExistingKey(kHttpUserChoiceLatestProgIdPath,
                           L"OtherBrowserHTML");
  WriteProgIdToExistingKey(kHttpUserChoicePath, L"LegacyBrowserHTML");
  run_loop.Run();

  EXPECT_EQ(call_count, 1);
}

TEST_F(DefaultBrowserMonitorWinTest,
       RecoversWhenKeysCreatedAfterInitialStartMonitor) {
  InitFeatureAndTaskEnvironment(/*enable_feature=*/true);

  DefaultBrowserMonitor monitor;
  base::RunLoop run_loop;
  bool notified = false;
  auto subscription = monitor.RegisterDefaultBrowserChanged(base::BindRepeating(
      [](bool* flag, base::RunLoop* loop) {
        *flag = true;
        loop->Quit();
      },
      &notified, &run_loop));

  // Initial `StartMonitor()` runs before either watched key exists (0 keys).
  monitor.StartMonitor();

  // Create the parent key and retry `StartMonitor()`.
  CreateKey(kHttpAssociationPath);
  monitor.StartMonitor();

  CreateAndWriteProgId(kHttpUserChoiceLatestProgIdPath, L"OtherBrowserHTML");

  run_loop.Run();
  EXPECT_TRUE(notified);
}

TEST_F(DefaultBrowserMonitorWinTest,
       PicksUpSecondKeyCreatedAfterInitialStartMonitor) {
  InitFeatureAndTaskEnvironment(/*enable_feature=*/true);

  // Only http exists when `StartMonitor()` first runs (1 of 2 watched keys).
  CreateKey(kHttpAssociationPath);

  DefaultBrowserMonitor monitor;
  base::RunLoop run_loop;
  bool notified = false;
  auto subscription = monitor.RegisterDefaultBrowserChanged(base::BindRepeating(
      [](bool* flag, base::RunLoop* loop) {
        *flag = true;
        loop->Quit();
      },
      &notified, &run_loop));
  monitor.StartMonitor();

  // Create https later and call `StartMonitor()` again; the watcher should
  // upgrade to watch both keys.
  CreateKey(kHttpsAssociationPath);
  monitor.StartMonitor();

  CreateAndWriteProgId(kHttpsUserChoiceLatestProgIdPath, L"OtherBrowserHTML");

  run_loop.Run();
  EXPECT_TRUE(notified);
}

TEST_F(DefaultBrowserMonitorWinTest, DetectsSubsequentChanges) {
  InitFeatureAndTaskEnvironment(/*enable_feature=*/true);

  CreateKey(kHttpUserChoiceLatestProgIdPath);

  DefaultBrowserMonitor monitor;
  int call_count = 0;
  base::RunLoop loop1;
  base::RunLoop loop2;
  auto subscription = monitor.RegisterDefaultBrowserChanged(base::BindRepeating(
      [](int* count, base::RunLoop* first_loop, base::RunLoop* second_loop) {
        ++(*count);
        if (*count == 1) {
          first_loop->Quit();
        } else if (*count == 2) {
          second_loop->Quit();
        }
      },
      &call_count, &loop1, &loop2));
  monitor.StartMonitor();

  WriteProgIdToExistingKey(kHttpUserChoiceLatestProgIdPath, L"BrowserOneHTML");
  loop1.Run();
  ASSERT_EQ(call_count, 1);

  WriteProgIdToExistingKey(kHttpUserChoiceLatestProgIdPath, L"BrowserTwoHTML");
  loop2.Run();
  EXPECT_EQ(call_count, 2);
}

class DefaultBrowserMonitorWinDebounceTest : public testing::Test {
 protected:
  DefaultBrowserMonitorWinDebounceTest() {
    feature_list_.InitAndEnableFeature(kDefaultBrowserMonitorUrlAssociations);
    task_environment_.emplace(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  void ConfigureFakeWatcher(DefaultBrowserMonitor& monitor) {
    monitor.SetRegistryWatcherFactoryForTesting(base::BindLambdaForTesting(
        [this](const std::vector<std::wstring>& key_paths,
               base::OnceClosure on_change_callback)
            -> std::unique_ptr<RegistryWatcher> {
          watched_key_paths_ = key_paths;
          ++watch_start_count_;
          pending_on_change_ = std::move(on_change_callback);
          return nullptr;
        }));
  }

  void SimulateRegistryChange() {
    ASSERT_TRUE(pending_on_change_);
    std::move(pending_on_change_).Run();
  }

  int watch_start_count() const { return watch_start_count_; }
  const std::vector<std::wstring>& watched_key_paths() const {
    return watched_key_paths_;
  }
  base::test::TaskEnvironment& task_environment() { return *task_environment_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::optional<base::test::TaskEnvironment> task_environment_;
  std::vector<std::wstring> watched_key_paths_;
  int watch_start_count_ = 0;
  base::OnceClosure pending_on_change_;
};

TEST_F(DefaultBrowserMonitorWinDebounceTest,
       DebouncesBurstOfWritesIntoSingleNotification) {
  DefaultBrowserMonitor monitor;
  ConfigureFakeWatcher(monitor);
  int call_count = 0;
  auto subscription = monitor.RegisterDefaultBrowserChanged(
      base::BindRepeating([](int* count) { ++(*count); }, &call_count));

  monitor.StartMonitor();
  EXPECT_EQ(watch_start_count(), 1);
  EXPECT_EQ(
      watched_key_paths(),
      (std::vector<std::wstring>{kHttpAssociationPath, kHttpsAssociationPath}));

  const base::TimeDelta debounce = kMonitorDebounceDelay.Get();
  ASSERT_GT(debounce, base::Milliseconds(10));

  // First change re-arms the watcher immediately and starts the debounce timer.
  SimulateRegistryChange();
  EXPECT_EQ(watch_start_count(), 2);
  task_environment().FastForwardBy(debounce - base::Milliseconds(10));
  EXPECT_EQ(call_count, 0);

  // Second change re-arms again and resets the trailing-edge debounce timer.
  SimulateRegistryChange();
  EXPECT_EQ(watch_start_count(), 3);
  task_environment().FastForwardBy(debounce - base::Milliseconds(1));
  EXPECT_EQ(call_count, 0);

  // Once the quiet period completes, observers are notified exactly once.
  task_environment().FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(call_count, 1);

  // A subsequent write after settling triggers a fresh debounced notification.
  SimulateRegistryChange();
  EXPECT_EQ(watch_start_count(), 4);
  task_environment().FastForwardBy(debounce);
  EXPECT_EQ(call_count, 2);
}

}  // namespace default_browser
