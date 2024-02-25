// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_command_line_config_policy.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "components/component_updater/configurator_impl.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/configurator.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_query_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {

class ChromeComponentUpdaterConfiguratorTest : public testing::Test {
 public:
  ChromeComponentUpdaterConfiguratorTest() = default;

  ChromeComponentUpdaterConfiguratorTest(
      const ChromeComponentUpdaterConfiguratorTest&) = delete;
  ChromeComponentUpdaterConfiguratorTest& operator=(
      const ChromeComponentUpdaterConfiguratorTest&) = delete;

  ~ChromeComponentUpdaterConfiguratorTest() override = default;

  // Overrides from testing::Test.
  void SetUp() override;

 protected:
  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

void ChromeComponentUpdaterConfiguratorTest::SetUp() {
  pref_service_ = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref_service_->registry());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestDisablePings) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "disable-pings");

  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));

  const std::vector<GURL> pingUrls = config->PingUrl();
  EXPECT_TRUE(pingUrls.empty());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestFastUpdate) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "fast-update");

  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));

  CHECK_EQ(base::Seconds(10), config->InitialDelay());
  CHECK_EQ(base::Hours(5), config->NextCheckDelay());
  CHECK_EQ(base::Seconds(2), config->OnDemandDelay());
  CHECK_EQ(base::Seconds(10), config->UpdateDelay());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestOverrideUrl) {
  const char overrideUrl[] = "http://0.0.0.0/";

  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());

  std::string val = "url-source";
  val.append("=");
  val.append(overrideUrl);
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, val.c_str());

  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));

  const std::vector<GURL> urls = config->UpdateUrl();

  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(overrideUrl, urls.at(0).possibly_invalid_spec());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestSwitchRequestParam) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  cmdline.AppendSwitchASCII(switches::kComponentUpdater, "test-request");

  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));

  EXPECT_FALSE(config->ExtraRequestParams().empty());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestUpdaterDefaultUrl) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));
  const auto urls = config->UpdateUrl();

  // Expect the default url to be cryptographically secure.
  EXPECT_GE(urls.size(), 1u);
  EXPECT_TRUE(urls.front().SchemeIsCryptographic());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestEnabledCupSigning) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));

  EXPECT_TRUE(config->EnabledCupSigning());
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestUseEncryption) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();

  {
    const auto config(
        MakeChromeComponentUpdaterConfigurator(cmdline, pref_service()));
    const auto urls = config->UpdateUrl();
    ASSERT_EQ(2u, urls.size());
    ASSERT_STREQ(kUpdaterJSONDefaultUrl, urls[0].spec().c_str());
    ASSERT_STREQ(kUpdaterJSONFallbackUrl, urls[1].spec().c_str());
    ASSERT_EQ(config->UpdateUrl(), config->PingUrl());
  }

  // Use the configurator implementation to test the filtering of
  // unencrypted URLs.
  {
    const ConfiguratorImpl config(
        ComponentUpdaterCommandLineConfigPolicy(cmdline), true);
    const auto urls = config.UpdateUrl();
    ASSERT_EQ(1u, urls.size());
    ASSERT_STREQ(kUpdaterJSONDefaultUrl, urls[0].spec().c_str());
    ASSERT_EQ(config.UpdateUrl(), config.PingUrl());
  }

  {
    const ConfiguratorImpl config(
        ComponentUpdaterCommandLineConfigPolicy(cmdline), false);
    const auto urls = config.UpdateUrl();
    ASSERT_EQ(2u, urls.size());
    ASSERT_STREQ(kUpdaterJSONDefaultUrl, urls[0].spec().c_str());
    ASSERT_STREQ(kUpdaterJSONFallbackUrl, urls[1].spec().c_str());
    ASSERT_EQ(config.UpdateUrl(), config.PingUrl());
  }
}

TEST_F(ChromeComponentUpdaterConfiguratorTest, TestProdId) {
  base::CommandLine cmdline(*base::CommandLine::ForCurrentProcess());
  const auto config(
      MakeChromeComponentUpdaterConfigurator(&cmdline, pref_service()));
  EXPECT_STREQ(update_client::UpdateQueryParams::GetProdIdString(
                   update_client::UpdateQueryParams::ProdId::CHROME),
               config->GetProdId().c_str());
}

}  // namespace component_updater
