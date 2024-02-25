// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using badging::BadgeManager;
using badging::BadgeManagerDelegate;

namespace {

typedef std::pair<GURL, std::optional<int>> SetBadgeAction;

constexpr uint64_t kBadgeContents = 1;
const webapps::AppId kAppId = "1";

}  // namespace

namespace badging {

class BadgeManagerUnittest : public ::testing::Test {
 public:
  BadgeManagerUnittest() = default;

  BadgeManagerUnittest(const BadgeManagerUnittest&) = delete;
  BadgeManagerUnittest& operator=(const BadgeManagerUnittest&) = delete;

  ~BadgeManagerUnittest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    // Delegate lifetime is managed by BadgeManager
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>(
        profile_.get(), &badge_manager());
    delegate_ = owned_delegate.get();
    badge_manager().SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override {
    // Clear raw_ptrs before `profile_` is reset to avoid a dangling pointer.
    delegate_ = nullptr;
    provider_ = nullptr;
    profile_.reset();
  }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  void set_delegate(TestBadgeManagerDelegate* delegate) {
    delegate_ = delegate;
  }

  BadgeManager& badge_manager() const {
    return *BadgeManagerFactory::GetForProfile(profile());
  }

  Profile* profile() const { return profile_.get(); }

  web_app::WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<web_app::FakeWebAppProvider> provider_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<TestBadgeManagerDelegate> delegate_;
};

TEST_F(BadgeManagerUnittest, SetFlagBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager().SetBadgeForTesting(kAppId, std::nullopt, &test_recorder);

  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kSetFlagBadge);

  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kAppId, delegate()->set_badges().front().first);
  EXPECT_EQ(std::nullopt, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     &test_recorder);
  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries[0],
                                  ukm::builders::Badging::kUpdateAppBadgeName,
                                  kSetNumericBadge);
  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kAppId, delegate()->set_badges().front().first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForMultipleApps) {
  const webapps::AppId kOtherAppId = "2";
  constexpr uint64_t kOtherContents = 2;

  std::vector<webapps::AppId> updated_apps;
  web_app::WebAppTestRegistryObserverAdapter observer(
      &provider().registrar_unsafe());
  observer.SetWebAppLastBadgingTimeChangedDelegate(base::BindLambdaForTesting(
      [&updated_apps](const webapps::AppId& app_id, const base::Time& time) {
        updated_apps.push_back(app_id);
      }));

  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(kOtherAppId,
                                     std::make_optional(kOtherContents),
                                     ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kOtherAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kOtherContents, delegate()->set_badges()[1].second);

  EXPECT_EQ(2UL, updated_apps.size());
  EXPECT_EQ(kAppId, updated_apps[0]);
  EXPECT_EQ(kOtherAppId, updated_apps[1]);
}

TEST_F(BadgeManagerUnittest, SetBadgeForAppAfterClear) {
  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, ClearBadgeForBadgedApp) {
  ukm::TestUkmRecorder test_recorder;

  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, &test_recorder);
  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kClearBadge);
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());
  EXPECT_EQ(kAppId, delegate()->cleared_badges().front());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(BadgeManagerUnittest, BadgingMultipleProfiles) {
  std::unique_ptr<Profile> other_profile = std::make_unique<TestingProfile>();
  web_app::FakeWebAppProvider* new_provider =
      web_app::FakeWebAppProvider::Get(other_profile.get());
  web_app::test::AwaitStartWebAppProviderAndSubsystems(other_profile.get());

  BadgeManager* other_badge_manager =
      BadgeManagerFactory::GetForProfile(other_profile.get());
  ASSERT_TRUE(other_badge_manager);

  auto owned_other_delegate = std::make_unique<TestBadgeManagerDelegate>(
      other_profile.get(), other_badge_manager);
  auto* other_delegate = owned_other_delegate.get();
  other_badge_manager->SetDelegate(std::move(owned_other_delegate));

  std::vector<webapps::AppId> updated_apps;
  std::vector<webapps::AppId> other_updated_apps;
  web_app::WebAppTestRegistryObserverAdapter other_observer(
      &new_provider->registrar_unsafe());
  other_observer.SetWebAppLastBadgingTimeChangedDelegate(
      base::BindLambdaForTesting(
          [&other_updated_apps](const webapps::AppId& app_id,
                                const base::Time& time) {
            other_updated_apps.push_back(app_id);
          }));
  web_app::WebAppTestRegistryObserverAdapter observer(
      &provider().registrar_unsafe());
  observer.SetWebAppLastBadgingTimeChangedDelegate(base::BindLambdaForTesting(
      [&updated_apps](const webapps::AppId& app_id, const base::Time& time) {
        updated_apps.push_back(app_id);
      }));

  other_badge_manager->SetBadgeForTesting(kAppId, std::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(
      kAppId, std::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(kAppId, std::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->ClearBadgeForTesting(kAppId,
                                            ukm::TestUkmRecorder::Get());

  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());

  EXPECT_EQ(3UL, other_delegate->set_badges().size());
  EXPECT_EQ(0UL, delegate()->set_badges().size());

  EXPECT_EQ(1UL, other_delegate->cleared_badges().size());
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());

  EXPECT_EQ(kAppId, other_delegate->set_badges().back().first);
  EXPECT_EQ(std::nullopt, other_delegate->set_badges().back().second);

  EXPECT_EQ(1UL, updated_apps.size());
  EXPECT_EQ(kAppId, updated_apps[0]);

  EXPECT_FALSE(other_updated_apps.empty());
  EXPECT_EQ(kAppId, other_updated_apps[0]);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests methods which call into the badge manager delegate do not crash when
// the delegate is unset.
TEST_F(BadgeManagerUnittest, BadgingWithNoDelegateDoesNotCrash) {
  // Set the delegate to nullptr to avoid a dangling pointer.
  set_delegate(nullptr);
  badge_manager().SetDelegate(nullptr);

  badge_manager().SetBadgeForTesting(kAppId, std::nullopt,
                                     ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(kAppId, std::make_optional(kBadgeContents),
                                     ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
}

}  // namespace badging
