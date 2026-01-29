// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_typeref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/values.h"
#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_auth {

namespace {

const CFStringRef kInvalidPropID(CFSTR("INVALID_ID"));

}  // namespace

using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

class TestCFWrapper : public CFPreferencesObserver {
 public:
  explicit TestCFWrapper(base::RepeatingClosure* callback, Config* config)
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

  base::OnceCallback<Config()> GetReadConfigCallback() override {
    return base::BindOnce([](Config config) { return config; }, *config_);
  }

 private:
  raw_ptr<base::RepeatingClosure> callback_;
  raw_ptr<Config> config_;
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

  std::unique_ptr<CFPreferencesObserver> TestCFPreferenceFactory() {
    return std::make_unique<TestCFWrapper>(&notification_callback_, &config_);
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

  void SetHostsPropertyOverride(ScopedPropList hosts) {
    ScopedPropList extension_id_prop = base::apple::ScopedCFTypeRef(
        ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID);
    ScopedPropList team_id_prop = base::apple::ScopedCFTypeRef(
        ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID);
    SetConfigOverride(std::move(extension_id_prop), std::move(team_id_prop),
                      std::move(hosts));
  }

  void SetHostsPropertyOverride(const std::vector<std::string_view>& hosts) {
    SetHostsPropertyOverride(HostsToScopedPropList(hosts));
  }

  void SetConfigOverride(ScopedPropList extension_id,
                         ScopedPropList team_id,
                         ScopedPropList hosts) {
    config_ = CFPreferencesObserver::Config(
        std::move(extension_id), std::move(team_id), std::move(hosts));
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
  CFPreferencesObserver::Config config_{/*extension_id=*/ScopedPropList(),
                                        /*team_id=*/ScopedPropList(),
                                        /*hosts=*/ScopedPropList()};
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
  CFStringRef extension_id;
  CFStringRef team_id;
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

  const struct ConfigTestParams& params = GetParam();
  ScopedPropList extension_id_prop =
      base::apple::ScopedCFTypeRef(params.extension_id);
  ScopedPropList team_id_prop = base::apple::ScopedCFTypeRef(params.team_id);
  SetConfigOverride(std::move(extension_id_prop), std::move(team_id_prop),
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
        ConfigTestParams({kInvalidPropID,
                          ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID}),
        ConfigTestParams(
            {ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID,
             kInvalidPropID}),
        ConfigTestParams({nullptr,
                          ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID}),
        ConfigTestParams(
            {ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID,
             nullptr}),
        ConfigTestParams({nullptr, nullptr})));

}  // namespace enterprise_auth
