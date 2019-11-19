// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/plugin_metrics_provider.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

content::WebPluginInfo CreateFakePluginInfo(
    const std::string& name,
    const base::FilePath::CharType* path,
    const std::string& version) {
  content::WebPluginInfo plugin(base::UTF8ToUTF16(name),
                                base::FilePath(path),
                                base::UTF8ToUTF16(version),
                                base::string16());
  plugin.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  return plugin;
}

class PluginMetricsProviderTest : public ::testing::Test {
 protected:
  PluginMetricsProviderTest()
      : prefs_(new TestingPrefServiceSimple) {
    PluginMetricsProvider::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() {
    return prefs_.get();
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;

  DISALLOW_COPY_AND_ASSIGN(PluginMetricsProviderTest);
};

}  // namespace

TEST_F(PluginMetricsProviderTest, IsPluginProcess) {
  EXPECT_TRUE(PluginMetricsProvider::IsPluginProcess(
      content::PROCESS_TYPE_PPAPI_PLUGIN));
  EXPECT_FALSE(PluginMetricsProvider::IsPluginProcess(
      content::PROCESS_TYPE_GPU));
}

TEST_F(PluginMetricsProviderTest, Plugins) {
  content::BrowserTaskEnvironment task_environment;

  PluginMetricsProvider provider(prefs());

  std::vector<content::WebPluginInfo> plugins;
  plugins.push_back(CreateFakePluginInfo("p1", FILE_PATH_LITERAL("p1.plugin"),
                                         "1.5"));
  plugins.push_back(CreateFakePluginInfo("p2", FILE_PATH_LITERAL("p2.plugin"),
                                         "2.0"));
  provider.SetPluginsForTesting(plugins);

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_EQ(2, system_profile.plugin_size());
  EXPECT_EQ("p1", system_profile.plugin(0).name());
  EXPECT_EQ("p1.plugin", system_profile.plugin(0).filename());
  EXPECT_EQ("1.5", system_profile.plugin(0).version());
  EXPECT_TRUE(system_profile.plugin(0).is_pepper());
  EXPECT_EQ("p2", system_profile.plugin(1).name());
  EXPECT_EQ("p2.plugin", system_profile.plugin(1).filename());
  EXPECT_EQ("2.0", system_profile.plugin(1).version());
  EXPECT_TRUE(system_profile.plugin(1).is_pepper());

  // Now set some plugin stability stats for p2 and verify they're recorded.
  std::unique_ptr<base::DictionaryValue> plugin_dict(new base::DictionaryValue);
  plugin_dict->SetString(prefs::kStabilityPluginName, "p2");
  plugin_dict->SetInteger(prefs::kStabilityPluginLaunches, 1);
  plugin_dict->SetInteger(prefs::kStabilityPluginCrashes, 2);
  plugin_dict->SetInteger(prefs::kStabilityPluginInstances, 3);
  plugin_dict->SetInteger(prefs::kStabilityPluginLoadingErrors, 4);
  {
    ListPrefUpdate update(prefs(), prefs::kStabilityPluginStats);
    update.Get()->Append(std::move(plugin_dict));
  }

  provider.ProvideStabilityMetrics(&system_profile);

  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();
  ASSERT_EQ(1, stability.plugin_stability_size());
  EXPECT_EQ("p2", stability.plugin_stability(0).plugin().name());
  EXPECT_EQ("p2.plugin", stability.plugin_stability(0).plugin().filename());
  EXPECT_EQ("2.0", stability.plugin_stability(0).plugin().version());
  EXPECT_TRUE(stability.plugin_stability(0).plugin().is_pepper());
  EXPECT_EQ(1, stability.plugin_stability(0).launch_count());
  EXPECT_EQ(2, stability.plugin_stability(0).crash_count());
  EXPECT_EQ(3, stability.plugin_stability(0).instance_count());
  EXPECT_EQ(4, stability.plugin_stability(0).loading_error_count());
}

TEST_F(PluginMetricsProviderTest, RecordCurrentStateWithDelay) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  PluginMetricsProvider provider(prefs());

  EXPECT_TRUE(provider.RecordCurrentStateWithDelay());
  EXPECT_FALSE(provider.RecordCurrentStateWithDelay());

  task_environment.FastForwardBy(PluginMetricsProvider::GetRecordStateDelay());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider.RecordCurrentStateWithDelay());
}

TEST_F(PluginMetricsProviderTest, RecordCurrentStateIfPending) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  PluginMetricsProvider provider(prefs());

  // First there should be no need to force RecordCurrentState.
  EXPECT_FALSE(provider.RecordCurrentStateIfPending());

  // After delayed task is posted RecordCurrentStateIfPending should return
  // true.
  EXPECT_TRUE(provider.RecordCurrentStateWithDelay());
  EXPECT_TRUE(provider.RecordCurrentStateIfPending());

  // If RecordCurrentStateIfPending was successful then we should be able to
  // post a new delayed task.
  EXPECT_TRUE(provider.RecordCurrentStateWithDelay());
}

TEST_F(PluginMetricsProviderTest, ProvideStabilityMetricsWhenPendingTask) {
  content::BrowserTaskEnvironment task_environment;

  PluginMetricsProvider provider(prefs());

  // Create plugin information for testing.
  std::vector<content::WebPluginInfo> plugins;
  plugins.push_back(
      CreateFakePluginInfo("p1", FILE_PATH_LITERAL("p1.plugin"), "1.5"));
  plugins.push_back(
      CreateFakePluginInfo("p2", FILE_PATH_LITERAL("p2.plugin"), "1.5"));
  provider.SetPluginsForTesting(plugins);
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  // Increase number of process launches which should also start a delayed
  // task.
  content::ChildProcessTerminationInfo abnormal_termination_info;
  abnormal_termination_info.status =
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION;
  abnormal_termination_info.exit_code = 1;
  content::ChildProcessData child_process_data1(
      content::PROCESS_TYPE_PPAPI_PLUGIN);
  child_process_data1.name = base::UTF8ToUTF16("p1");
  provider.BrowserChildProcessHostConnected(child_process_data1);
  provider.BrowserChildProcessCrashed(child_process_data1,
                                      abnormal_termination_info);

  // A disconnect should not generate a crash event.
  provider.BrowserChildProcessHostConnected(child_process_data1);
  provider.BrowserChildProcessHostDisconnected(child_process_data1);

  content::ChildProcessData child_process_data2(
      content::PROCESS_TYPE_PPAPI_PLUGIN);
  child_process_data2.name = base::UTF8ToUTF16("p2");
  provider.BrowserChildProcessHostConnected(child_process_data2);
  provider.BrowserChildProcessCrashed(child_process_data2,
                                      abnormal_termination_info);

  // A kill should generate a crash event
  provider.BrowserChildProcessHostConnected(child_process_data2);
  provider.BrowserChildProcessKilled(child_process_data2,
                                     abnormal_termination_info);

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();
  size_t found = 0;
  EXPECT_EQ(stability.plugin_stability_size(), 2);
  for (int i = 0; i < 2; i++) {
    std::string name = stability.plugin_stability(i).plugin().name();
    if (name == "p1") {
      EXPECT_EQ(2, stability.plugin_stability(i).launch_count());
      EXPECT_EQ(1, stability.plugin_stability(i).crash_count());
      found++;
    } else if (name == "p2") {
      EXPECT_EQ(2, stability.plugin_stability(i).launch_count());
      EXPECT_EQ(2, stability.plugin_stability(i).crash_count());
      found++;
    } else {
      GTEST_FAIL() << "Unexpected plugin name : " << name;
    }
  }
  EXPECT_EQ(found, 2U);
}
