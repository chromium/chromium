// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

#include <array>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_typeref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/values.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_auth {

namespace {

constexpr char kInvalidPropID[] = "INVALID_ID";
constexpr char kEmptyPropID[] = "";
constexpr char kExampleHost[] = "example.com";
constexpr char kWildcardHost[] = ".foo.net";
// Arbitrary non-string value used to verify that such `Hosts` entries are
// dropped.
constexpr int kNonStringHostEntry = 42;

using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

// Wraps `value` in a CFString property list, or returns an empty scoper when
// `value` is null to simulate a missing MDM payload entry.
ScopedPropList StringToScopedPropList(const char* value) {
  if (!value) {
    return ScopedPropList();
  }
  return base::SysUTF8ToCFStringRef(value);
}

ScopedPropList HostsToScopedPropList(
    const std::vector<std::string_view>& hosts) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> res(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  for (const auto& value : hosts) {
    base::apple::ScopedCFTypeRef<CFStringRef> host =
        base::SysUTF8ToCFStringRef(value);
    CFArrayAppendValue(res.get(), host.get());
  }
  return res;
}

// Returns a `Hosts` array mixing a valid host with an empty string and a
// non-string entry, both of which the parser is expected to drop.
ScopedPropList MixedHostsToScopedPropList() {
  base::apple::ScopedCFTypeRef<CFStringRef> valid_host =
      base::SysUTF8ToCFStringRef(kExampleHost);
  base::apple::ScopedCFTypeRef<CFStringRef> empty_host =
      base::SysUTF8ToCFStringRef(kEmptyPropID);
  base::apple::ScopedCFTypeRef<CFNumberRef> non_string_host(CFNumberCreate(
      kCFAllocatorDefault, kCFNumberIntType, &kNonStringHostEntry));
  std::array<CFTypeRef, 3> values = {valid_host.get(), empty_host.get(),
                                     non_string_host.get()};
  return base::apple::ScopedCFTypeRef<CFArrayRef>(CFArrayCreate(
      kCFAllocatorDefault, values.data(), static_cast<CFIndex>(values.size()),
      &kCFTypeArrayCallBacks));
}

}  // namespace

class TestCFWrapper : public CFPreferencesObserver {
 public:
  explicit TestCFWrapper(base::RepeatingClosure* callback, RawConfig* config)
      : callback_(callback), config_(config) {}

  void Subscribe(base::RepeatingClosure on_update) override {
    if (!*callback_) {
      *callback_ = std::move(on_update);
    }
  }

  void Unsubscribe() override {
    if (*callback_) {
      callback_->Reset();
    }
  }

  base::OnceCallback<RawConfig()> GetReadConfigCallback() override {
    return base::BindOnce([](RawConfig config) { return config; }, *config_);
  }

 private:
  raw_ptr<base::RepeatingClosure> callback_;
  raw_ptr<RawConfig> config_;
};

class ExtensibleEnterpriseSSOPrefsHandlerTest : public testing::Test {
 protected:
  ExtensibleEnterpriseSSOPrefsHandlerTest() {
    ExtensibleEnterpriseSSOPrefsHandler::RegisterPrefs(
        pref_service_.registry());

    cf_prefs_override_.emplace(base::BindRepeating(
        &ExtensibleEnterpriseSSOPrefsHandlerTest::TestCFPreferenceFactory,
        base::Unretained(this)));

    prefs_handler_ =
        std::make_unique<ExtensibleEnterpriseSSOPrefsHandler>(&pref_service_);
  }

  static std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config> ParseConfig(
      CFPreferencesObserver::RawConfig raw_config) {
    return ExtensibleEnterpriseSSOPrefsHandler::ParseConfig(
        std::move(raw_config));
  }

  std::unique_ptr<CFPreferencesObserver> TestCFPreferenceFactory() {
    return std::make_unique<TestCFWrapper>(&notification_callback_, &config_);
  }

  void SetHostsPropertyOverride(ScopedPropList hosts) {
    SetConfigOverride(StringToScopedPropList(kOktaSsoExtensionId),
                      StringToScopedPropList(kOktaSsoTeamId), std::move(hosts));
  }

  void SetHostsPropertyOverride(const std::vector<std::string_view>& hosts) {
    SetHostsPropertyOverride(HostsToScopedPropList(hosts));
  }

  void SetConfigOverride(ScopedPropList extension_id,
                         ScopedPropList team_id,
                         ScopedPropList hosts) {
    config_ = CFPreferencesObserver::RawConfig{
        .extension_id = std::move(extension_id),
        .team_id = std::move(team_id),
        .hosts = std::move(hosts),
    };
  }

  void WaitUntilPrefsChange(size_t new_size) {
    base::test::RunUntil([this, new_size]() {
      return new_size ==
             pref_service_
                 .GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
                 .size();
    });
  }

  void SendNotification() {
    if (notification_callback_) {
      notification_callback_.Run();
    }
  }

  void TearDown() override {
    cf_prefs_override_.reset();
    prefs_handler_.reset();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  CFPreferencesObserver::RawConfig config_;
  std::unique_ptr<ExtensibleEnterpriseSSOPrefsHandler> prefs_handler_;
  base::RepeatingClosure notification_callback_;
  TestingPrefServiceSimple pref_service_;
  std::optional<ScopedCFPreferenceObserverOverride> cf_prefs_override_;
};

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, BasicList) {
  const std::vector<std::string_view> hosts = {"foo.bar.com", "example.net",
                                               "foo bar foo bar"};
  SetHostsPropertyOverride(hosts);

  prefs_handler_->UpdatePrefs();
  WaitUntilPrefsChange(hosts.size());

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  for (const auto& host : hosts) {
    EXPECT_TRUE(result.contains(host));
  }
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, WithEmptyList) {
  const std::vector<std::string_view> hosts;
  SetHostsPropertyOverride(hosts);

  base::ListValue default_list;
  default_list.Append("example.com");
  // Setup a non-empty default value so we can wait until it becomes empty.
  pref_service_.SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts,
                        std::move(default_list));

  prefs_handler_->UpdatePrefs();
  WaitUntilPrefsChange(0);

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  EXPECT_TRUE(result.empty());
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, PropertyNotFound) {
  SetHostsPropertyOverride(ScopedPropList());

  base::ListValue default_list;
  default_list.Append("example.com");
  // Setup a non-empty default value so we can wait until it becomes empty.
  pref_service_.SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts,
                        std::move(default_list));

  prefs_handler_->UpdatePrefs();
  WaitUntilPrefsChange(0);

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  EXPECT_TRUE(result.empty());
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, InvalidPoperty) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> props(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  CFArrayAppendValue(props.get(), kCFBooleanTrue);
  CFArrayAppendValue(props.get(),
                     base::SysUTF8ToCFStringRef("example.com").get());
  int value = 42;
  base::apple::ScopedCFTypeRef<CFNumberRef> number(
      CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value));
  CFArrayAppendValue(props.get(), number.get());
  SetHostsPropertyOverride(std::move(props));

  prefs_handler_->UpdatePrefs();
  WaitUntilPrefsChange(1);

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  EXPECT_TRUE(result.contains("example.com"));
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, UpdatesOnNotification) {
  const std::vector<std::string_view> hosts = {"foo.bar.com", "example.net",
                                               "foo bar foo bar"};
  SetHostsPropertyOverride(hosts);

  SendNotification();
  WaitUntilPrefsChange(hosts.size());

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  for (const auto& host : hosts) {
    EXPECT_TRUE(result.contains(host));
  }
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, CorrectlyStopsListening) {
  const std::vector<std::string_view> hosts = {"foo.bar.com", "example.net",
                                               "foo bar foo bar"};
  SetHostsPropertyOverride(hosts);

  SendNotification();
  WaitUntilPrefsChange(hosts.size());

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  for (const auto& host : hosts) {
    EXPECT_TRUE(result.contains(host));
  }

  // Stop observing.
  const std::vector<std::string_view> new_hosts = {"example.com"};
  SetHostsPropertyOverride(new_hosts);
  prefs_handler_.reset();

  SendNotification();

  // Make sure that hosts are still the same.
  const base::ListValue& new_result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  for (const auto& host : hosts) {
    EXPECT_TRUE(new_result.contains(host));
  }
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, OnlyStopObserving) {
  std::vector<std::string_view> hosts = {"foobar.com"};
  SetHostsPropertyOverride(hosts);
  prefs_handler_.reset();

  SendNotification();
  // Since nothing should happen we just wait until the queue is idle.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
          .empty());
}

TEST_F(ExtensibleEnterpriseSSOPrefsHandlerTest, StopAndStartAgain) {
  const std::vector<std::string_view> hosts = {"foo.bar.com", "example.net",
                                               "foo bar foo bar"};
  SetHostsPropertyOverride(hosts);
  prefs_handler_.reset();

  SendNotification();
  // Since nothing should happen we just wait until the queue is idle.
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
          .empty());

  prefs_handler_ =
      std::make_unique<ExtensibleEnterpriseSSOPrefsHandler>(&pref_service_);

  SetHostsPropertyOverride(hosts);
  SendNotification();
  WaitUntilPrefsChange(hosts.size());

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  for (const auto& host : hosts) {
    EXPECT_TRUE(result.contains(host));
  }
}

struct ConfigTestParams {
  // Null simulates a missing payload entry.
  const char* extension_id;
  const char* team_id;
};

class ExtensibleEnterpriseSSOPrefsHandlerConfigTest
    : public ExtensibleEnterpriseSSOPrefsHandlerTest,
      public testing::WithParamInterface<ConfigTestParams> {};

TEST_P(ExtensibleEnterpriseSSOPrefsHandlerConfigTest,
       IgnoredConfigsWithInvalidIDs) {
  const std::vector<std::string_view> hosts = {"example.com", "foo.bar.net"};

  base::ListValue default_list;
  default_list.Append("example.net");
  // Setup a non-empty default value so we can wait until it becomes empty.
  pref_service_.SetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts,
                        std::move(default_list));

  const ConfigTestParams& params = GetParam();
  SetConfigOverride(StringToScopedPropList(params.extension_id),
                    StringToScopedPropList(params.team_id),
                    HostsToScopedPropList(hosts));
  prefs_handler_->UpdatePrefs();
  WaitUntilPrefsChange(0);

  const base::ListValue& result =
      pref_service_.GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  ASSERT_TRUE(result.empty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensibleEnterpriseSSOPrefsHandlerConfigTest,
    testing::Values(
        ConfigTestParams({kInvalidPropID, kOktaSsoTeamId}),
        ConfigTestParams({kOktaSsoExtensionId, kInvalidPropID}),
        ConfigTestParams({nullptr, kOktaSsoTeamId}),
        ConfigTestParams({kOktaSsoExtensionId, nullptr}),
        ConfigTestParams({nullptr, nullptr}),
        ConfigTestParams({kEmptyPropID, kOktaSsoTeamId}),
        ConfigTestParams({kOktaSsoExtensionId, kEmptyPropID}),
        ConfigTestParams({kEmptyPropID, kEmptyPropID}),
        // Microsoft is a supported IdP, but host syncing is Okta-only.
        ConfigTestParams({kMicrosoftSsoExtensionId, kMicrosoftSsoTeamId}),
        // IDs from different IdPs must not be combined.
        ConfigTestParams({kOktaSsoExtensionId, kMicrosoftSsoTeamId}),
        ConfigTestParams({kMicrosoftSsoExtensionId, kOktaSsoTeamId})));

using Config = ExtensibleEnterpriseSSOPrefsHandler::Config;

// Makes `Config` mismatches readable in test failure output.
void PrintTo(const Config& config, std::ostream* os) {
  *os << "{extension_id: \"" << config.extension_id << "\", team_id: \""
      << config.team_id << "\", hosts: ";
  if (config.hosts.has_value()) {
    *os << "[" << base::JoinString(config.hosts.value(), ", ") << "]";
  } else {
    *os << "nullopt";
  }
  *os << "}";
}

struct ParseConfigTestParams {
  // Used as the test name suffix, must be alphanumeric.
  const char* name;
  CFPreferencesObserver::RawConfig raw_config;
  // `std::nullopt` when `raw_config` is expected to be rejected.
  std::optional<Config> expected_config;
};

class ExtensibleEnterpriseSSOParseConfigTest
    : public ExtensibleEnterpriseSSOPrefsHandlerTest,
      public testing::WithParamInterface<ParseConfigTestParams> {};

// Tests that `ParseConfig` converts each `RawConfig` into the expected
// `Config`, or rejects it.
TEST_P(ExtensibleEnterpriseSSOParseConfigTest, ParsesRawConfig) {
  const ParseConfigTestParams& params = GetParam();
  EXPECT_EQ(ParseConfig(params.raw_config), params.expected_config);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensibleEnterpriseSSOParseConfigTest,
    testing::Values(
        ParseConfigTestParams{
            .name = "ValidOkta",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = HostsToScopedPropList({kExampleHost,
                                                    kWildcardHost}),
                },
            .expected_config =
                Config{
                    .extension_id = kOktaSsoExtensionId,
                    .team_id = kOktaSsoTeamId,
                    .hosts = std::vector<std::string>{kExampleHost,
                                                      kWildcardHost},
                },
        },
        ParseConfigTestParams{
            .name = "DropsInvalidHostEntries",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = MixedHostsToScopedPropList(),
                },
            .expected_config =
                Config{
                    .extension_id = kOktaSsoExtensionId,
                    .team_id = kOktaSsoTeamId,
                    .hosts = std::vector<std::string>{kExampleHost},
                },
        },
        ParseConfigTestParams{
            .name = "EmptyHosts",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = HostsToScopedPropList({}),
                },
            // An empty `Hosts` array is distinct from a missing one.
            .expected_config =
                Config{
                    .extension_id = kOktaSsoExtensionId,
                    .team_id = kOktaSsoTeamId,
                    .hosts = std::vector<std::string>(),
                },
        },
        // `Hosts` is optional, so a malformed entry doesn't invalidate the
        // whole payload.
        ParseConfigTestParams{
            .name = "HostsNotAnArray",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = StringToScopedPropList(kExampleHost),
                },
            .expected_config =
                Config{
                    .extension_id = kOktaSsoExtensionId,
                    .team_id = kOktaSsoTeamId,
                    .hosts = std::nullopt,
                },
        },
        // `Hosts` is optional, e.g. for `Redirect` payloads.
        ParseConfigTestParams{
            .name = "MissingHosts",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = ScopedPropList(),
                },
            .expected_config =
                Config{
                    .extension_id = kOktaSsoExtensionId,
                    .team_id = kOktaSsoTeamId,
                    .hosts = std::nullopt,
                },
        },
        ParseConfigTestParams{
            .name = "MissingExtensionId",
            .raw_config =
                {
                    .extension_id = ScopedPropList(),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        },
        ParseConfigTestParams{
            .name = "MissingTeamId",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = ScopedPropList(),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        },
        ParseConfigTestParams{
            .name = "EmptyExtensionId",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kEmptyPropID),
                    .team_id = StringToScopedPropList(kOktaSsoTeamId),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        },
        ParseConfigTestParams{
            .name = "NonStringTeamId",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = HostsToScopedPropList({kOktaSsoTeamId}),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        },
        // Microsoft is a supported IdP, but host syncing is Okta-only.
        ParseConfigTestParams{
            .name = "MicrosoftIdp",
            .raw_config =
                {
                    .extension_id =
                        StringToScopedPropList(kMicrosoftSsoExtensionId),
                    .team_id = StringToScopedPropList(kMicrosoftSsoTeamId),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        },
        ParseConfigTestParams{
            .name = "MismatchedIdps",
            .raw_config =
                {
                    .extension_id = StringToScopedPropList(kOktaSsoExtensionId),
                    .team_id = StringToScopedPropList(kMicrosoftSsoTeamId),
                    .hosts = HostsToScopedPropList({kExampleHost}),
                },
            .expected_config = std::nullopt,
        }),
    [](const testing::TestParamInfo<ParseConfigTestParams>& info) {
      return std::string(info.param.name);
    });

}  // namespace enterprise_auth
