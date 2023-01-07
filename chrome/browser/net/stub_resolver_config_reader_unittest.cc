// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/stub_resolver_config_reader.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kDohConfigString[] =
    "https://doh1.test https://doh2.test/query{?dns}";

// Override the reader to mock out the ShouldDisableDohFor...() methods.
class MockedStubResolverConfigReader : public StubResolverConfigReader {
 public:
  explicit MockedStubResolverConfigReader(PrefService* local_state)
      : StubResolverConfigReader(local_state,
                                 false /* set_up_pref_defaults */) {}

  bool ShouldDisableDohForManaged() override { return disable_for_managed_; }

  bool ShouldDisableDohForParentalControls() override {
    parental_controls_checked_ = true;
    return disable_for_parental_controls_;
  }

  void set_disable_for_managed() { disable_for_managed_ = true; }

  void set_disable_for_parental_controls() {
    disable_for_parental_controls_ = true;
  }

  bool parental_controls_checked() { return parental_controls_checked_; }

 private:
  bool disable_for_managed_ = false;
  bool disable_for_parental_controls_ = false;

  bool parental_controls_checked_ = false;
};

class StubResolverConfigReaderTest : public testing::Test {
 public:
  StubResolverConfigReaderTest() {
    StubResolverConfigReader::RegisterPrefs(local_state_.registry());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<MockedStubResolverConfigReader> config_reader_ =
      std::make_unique<MockedStubResolverConfigReader>(&local_state_);

  const net::DnsOverHttpsConfig expected_doh_config_ =
      *net::DnsOverHttpsConfig::FromString(kDohConfigString);
};

TEST_F(StubResolverConfigReaderTest, GetSecureDnsConfiguration) {
  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_FALSE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DohEnabled) {
  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(expected_doh_config_, secure_dns_config.doh_servers());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DohEnabled_Secure) {
  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  // |force_check_parental_controls_for_automatic_mode| should have no effect on
  // SECURE mode, so set to false to ensure check is not deferred.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kSecure, secure_dns_config.mode());
  EXPECT_EQ(expected_doh_config_, secure_dns_config.doh_servers());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForManaged) {
  config_reader_->set_disable_for_managed();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForManaged_Secure) {
  config_reader_->set_disable_for_managed();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  // Parental controls should not be checked when DoH otherwise disabled.
  EXPECT_FALSE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForParentalControls) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  // |force_check_parental_controls_for_automatic_mode = true| is not the main
  // default case, but the specific behavior involved is tested separately.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      true /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DisabledForParentalControls_Secure) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeSecure);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  // |force_check_parental_controls_for_automatic_mode| should have no effect on
  // SECURE mode, so set to false to ensure check is not deferred.
  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());

  EXPECT_TRUE(config_reader_->parental_controls_checked());
}

TEST_F(StubResolverConfigReaderTest, DeferredParentalControlsCheck) {
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetString(prefs::kDnsOverHttpsMode,
                         SecureDnsConfig::kModeAutomatic);
  local_state_.SetString(prefs::kDnsOverHttpsTemplates, kDohConfigString);

  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  // Parental controls check initially skipped.
  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(expected_doh_config_, secure_dns_config.doh_servers());
  EXPECT_FALSE(config_reader_->parental_controls_checked());

  task_environment_.AdvanceClock(
      StubResolverConfigReader::kParentalControlsCheckDelay);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(config_reader_->parental_controls_checked());

  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kOff, secure_dns_config.mode());
  EXPECT_THAT(secure_dns_config.doh_servers().servers(), testing::IsEmpty());
}

TEST_F(StubResolverConfigReaderTest, DeferredParentalControlsCheck_Managed) {
  config_reader_->set_disable_for_managed();
  config_reader_->set_disable_for_parental_controls();

  local_state_.SetBoolean(prefs::kBuiltInDnsClientEnabled, true);
  local_state_.SetManagedPref(
      prefs::kDnsOverHttpsMode,
      std::make_unique<base::Value>(SecureDnsConfig::kModeAutomatic));
  local_state_.SetManagedPref(prefs::kDnsOverHttpsTemplates,
                              std::make_unique<base::Value>(kDohConfigString));

  SecureDnsConfig secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  // Parental controls check initially skipped, and managed prefs take
  // precedence over disables.
  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(expected_doh_config_, secure_dns_config.doh_servers());
  EXPECT_FALSE(config_reader_->parental_controls_checked());

  task_environment_.AdvanceClock(
      StubResolverConfigReader::kParentalControlsCheckDelay);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(config_reader_->parental_controls_checked());

  secure_dns_config = config_reader_->GetSecureDnsConfiguration(
      false /* force_check_parental_controls_for_automatic_mode */);

  // Expect DoH still enabled after parental controls check because managed
  // prefs have precedence.
  EXPECT_TRUE(config_reader_->GetInsecureStubResolverEnabled());
  EXPECT_EQ(net::SecureDnsMode::kAutomatic, secure_dns_config.mode());
  EXPECT_EQ(expected_doh_config_, secure_dns_config.doh_servers());
}

}  // namespace
