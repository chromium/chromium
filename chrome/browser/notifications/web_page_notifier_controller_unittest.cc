// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_page_notifier_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/test/content_settings_mock_provider.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Note that so far it is only working for full URLs with a scheme e.g. https.
// For every other pattern type it doesn't work (without a scheme or with
// wildcards).
// TODO(tomdobro): enable other test cases after the problem is fixed.
constexpr const char* kTestPatterns[] = {
    "https://full.test.com",
    // "no_scheme.test.com",
    // "[*.]any.test.com",
    // "*://any_scheme.test.com",
    // "https://[*.]scheme_any.test.com",
    // "*://[*.]any_any.test.com",
};

class MockObserver : public NotifierController::Observer {
 public:
  MockObserver() = default;

  MOCK_METHOD2(OnIconImageUpdated,
               void(const message_center::NotifierId& id,
                    const gfx::ImageSkia& image));

  MOCK_METHOD2(OnNotifierEnabledChanged,
               void(const message_center::NotifierId& id, bool enabled));
};

std::unique_ptr<KeyedService> BuildMockFaviconService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<favicon::MockFaviconService>>();
}

}  // namespace

class WebPageNotifierControllerTest : public testing::Test {
 protected:
  void TestGetNotifiersList(ContentSetting content_setting,
                            content_settings::ProviderType provider_type,
                            bool expect_enabled,
                            bool expect_enforced);

  content::BrowserTaskEnvironment task_environment_;

  MockObserver mock_observer_;
};

void WebPageNotifierControllerTest::TestGetNotifiersList(
    ContentSetting content_setting,
    content_settings::ProviderType provider_type,
    bool expect_enabled,
    bool expect_enforced) {
  WebPageNotifierController controller(&mock_observer_);
  std::unique_ptr<TestingProfile> profile;

  TestingProfile::Builder builder;
  builder.AddTestingFactory(FaviconServiceFactory::GetInstance(),
                            base::BindRepeating(&BuildMockFaviconService));
  profile = builder.Build();

  profile->SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(profile.get()));

  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile.get());

  auto provider = std::make_unique<content_settings::MockProvider>();

  for (const char* pattern : kTestPatterns) {
    provider->SetWebsiteSetting(
        ContentSettingsPattern::FromString(pattern),
        ContentSettingsPattern::Wildcard(), ContentSettingsType::NOTIFICATIONS,
        base::Value(content_setting), /*constraints=*/{},
        content_settings::PartitionKey::GetDefaultForTesting());
  }

  content_settings::TestUtils::OverrideProvider(
      host_content_settings_map, std::move(provider), provider_type);

  const auto list = controller.GetNotifierList(profile.get());

  for (const auto& el : list) {
    SCOPED_TRACE(el.name);
    EXPECT_EQ(expect_enabled, el.enabled);
    EXPECT_EQ(expect_enforced, el.enforced);
  }
}

TEST_F(WebPageNotifierControllerTest, TestGetNotifiersListPrefs) {
  // Test URL patterns as they were given by kPrefProvider imitating
  // notifications enabled by the user (as opposed to admin), thus not enforced.
  TestGetNotifiersList(CONTENT_SETTING_ALLOW,
                       content_settings::ProviderType::kPrefProvider,
                       /*expect_enabled=*/true, /*expect_enforced=*/false);
}

TEST_F(WebPageNotifierControllerTest, TestGetNotifiersListEnforced) {
  // Test URL patterns as they were given by kPolicyProvider imitating
  // notifications enabled by the admin, thus enforced.
  TestGetNotifiersList(CONTENT_SETTING_ALLOW,
                       content_settings::ProviderType::kPolicyProvider,
                       /*expect_enabled=*/true, /*expect_enforced=*/true);
}
