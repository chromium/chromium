// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_updater_configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_query_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using component_updater::ComponentUpdaterCommandLineConfigPolicy;
using update_client::Configurator;

namespace android_webview {

class AwComponentUpdaterConfiguratorTest : public testing::Test {
 public:
  AwComponentUpdaterConfiguratorTest() = default;
  ~AwComponentUpdaterConfiguratorTest() override = default;

  AwComponentUpdaterConfiguratorTest(
      const AwComponentUpdaterConfiguratorTest&) = delete;
  void operator=(const AwComponentUpdaterConfiguratorTest&) = delete;

  // Overrides from testing::Test.
  void SetUp() override;

 protected:
  TestingPrefServiceSimple* GetPrefService() { return pref_service_.get(); }
  base::CommandLine* GetCommandLine() { return cmdline_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<base::CommandLine> cmdline_;
};

void AwComponentUpdaterConfiguratorTest::SetUp() {
  pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref_service_->registry());
  cmdline_ = std::make_unique<base::CommandLine>(
      *base::CommandLine::ForCurrentProcess());
}

TEST_F(AwComponentUpdaterConfiguratorTest, TestDelays) {
  scoped_refptr<update_client::Configurator> config =
      MakeAwComponentUpdaterConfigurator(GetCommandLine(), GetPrefService());

  EXPECT_EQ(config->InitialDelay(), base::Seconds(10));
  EXPECT_EQ(config->NextCheckDelay(), base::Hours(5));
  EXPECT_EQ(config->OnDemandDelay(), base::Minutes(30));
  EXPECT_EQ(config->UpdateDelay(), base::Seconds(0));
}

TEST_F(AwComponentUpdaterConfiguratorTest, TestDelaysWithFastUpdate) {
  base::CommandLine* cmdline = GetCommandLine();
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, "fast-update");
  scoped_refptr<update_client::Configurator> config =
      MakeAwComponentUpdaterConfigurator(cmdline, GetPrefService());

  EXPECT_EQ(config->InitialDelay(), base::Seconds(10));
  EXPECT_EQ(config->NextCheckDelay(), base::Hours(5));
  EXPECT_EQ(config->OnDemandDelay(), base::Seconds(2));
  EXPECT_EQ(config->UpdateDelay(), base::Seconds(0));
}

TEST_F(AwComponentUpdaterConfiguratorTest, TestDefaultImpl) {
  // Test default implementation pumped from ConfiguratorImpl
  scoped_refptr<update_client::Configurator> config =
      MakeAwComponentUpdaterConfigurator(GetCommandLine(), GetPrefService());

  std::vector<GURL> urls = config->UpdateUrl();
  ASSERT_EQ(urls.size(), 2u);
  EXPECT_TRUE(urls.front().SchemeIsCryptographic());
  EXPECT_STREQ(component_updater::kUpdaterJSONDefaultUrl,
               urls[0].spec().c_str());
  EXPECT_STREQ(component_updater::kUpdaterJSONFallbackUrl,
               urls[1].spec().c_str());

  EXPECT_EQ(config->UpdateUrl(), config->PingUrl());

  EXPECT_TRUE(config->ExtraRequestParams().empty());
  EXPECT_TRUE(config->GetDownloadPreference().empty());

  EXPECT_TRUE(config->EnabledCupSigning());
  EXPECT_TRUE(config->EnabledDeltas());
  EXPECT_FALSE(config->EnabledBackgroundDownloader());
}

TEST_F(AwComponentUpdaterConfiguratorTest, TestCustomImpl) {
  // Test custom value specific for WebView.
  scoped_refptr<update_client::Configurator> config =
      MakeAwComponentUpdaterConfigurator(GetCommandLine(), GetPrefService());

  EXPECT_STREQ(update_client::UpdateQueryParams::GetProdIdString(
                   update_client::UpdateQueryParams::ProdId::WEBVIEW),
               config->GetProdId().c_str());

  EXPECT_TRUE(config->GetLang().empty());
}

TEST_F(AwComponentUpdaterConfiguratorTest, TestSwitchRequestParam) {
  base::CommandLine* cmdline = GetCommandLine();
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, "test-request");

  scoped_refptr<update_client::Configurator> config =
      MakeAwComponentUpdaterConfigurator(cmdline, GetPrefService());

  EXPECT_FALSE(config->ExtraRequestParams().empty());
}

}  // namespace android_webview
