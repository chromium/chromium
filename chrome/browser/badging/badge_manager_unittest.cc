// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/test/bind.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using badging::BadgeManager;
using badging::BadgeManagerDelegate;
using badging::BadgeManagerFactory;

namespace {

typedef std::pair<GURL, base::Optional<int>> SetBadgeAction;

constexpr uint64_t kBadgeContents = 1;
const web_app::AppId kAppId = "1";

}  // namespace

namespace badging {

class TestBadgingAppRegistryController
    : public web_app::TestAppRegistryController {
 public:
  TestBadgingAppRegistryController(Profile* profile,
                                   std::vector<web_app::AppId>& updated_apps)
      : web_app::TestAppRegistryController(profile),
        updated_apps_(updated_apps) {}
  ~TestBadgingAppRegistryController() override = default;

  void SetAppLastBadgingTime(const web_app::AppId& app_id,
                             const base::Time& time) override {
    updated_apps_.push_back(app_id);
  }

 private:
  std::vector<web_app::AppId>& updated_apps_;
};

class BadgeManagerUnittest : public ::testing::Test {
 public:
  BadgeManagerUnittest() = default;
  ~BadgeManagerUnittest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    StartTestWebAppProvider(profile(), updated_apps_);

    badge_manager_ =
        BadgeManagerFactory::GetInstance()->GetForProfile(profile_.get());

    // Delegate lifetime is managed by BadgeManager
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>(
        profile_.get(), badge_manager_);
    delegate_ = owned_delegate.get();
    badge_manager_->SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override { profile_.reset(); }

  static void StartTestWebAppProvider(
      Profile* profile,
      std::vector<web_app::AppId>& updated_apps) {
    auto* provider = web_app::TestWebAppProvider::Get(profile);
    std::unique_ptr<web_app::AppRegistryController> test_registry_controller(
        new TestBadgingAppRegistryController(profile, updated_apps));
    provider->SetRegistryController(std::move(test_registry_controller));
    provider->Start();
  }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  BadgeManager* badge_manager() const { return badge_manager_; }

  Profile* profile() const { return profile_.get(); }

  const std::vector<web_app::AppId>& updated_apps() const {
    return updated_apps_;
  }

 private:
  TestBadgeManagerDelegate* delegate_;
  BadgeManager* badge_manager_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::vector<web_app::AppId> updated_apps_;

  DISALLOW_COPY_AND_ASSIGN(BadgeManagerUnittest);
};

TEST_F(BadgeManagerUnittest, SetFlagBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager()->SetBadgeForTesting(kAppId, base::nullopt, &test_recorder);

  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kSetFlagBadge);

  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kAppId, delegate()->set_badges().front().first);
  EXPECT_EQ(base::nullopt, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), &test_recorder);
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
  const web_app::AppId kOtherAppId = "2";
  constexpr uint64_t kOtherContents = 2;

  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager()->SetBadgeForTesting(kOtherAppId,
                                      base::make_optional(kOtherContents),
                                      ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kOtherAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kOtherContents, delegate()->set_badges()[1].second);

  EXPECT_EQ(2UL, updated_apps().size());
  EXPECT_EQ(kAppId, updated_apps()[0]);
  EXPECT_EQ(kOtherAppId, updated_apps()[1]);
}

TEST_F(BadgeManagerUnittest, SetBadgeForAppAfterClear) {
  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager()->ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, ClearBadgeForBadgedApp) {
  ukm::TestUkmRecorder test_recorder;

  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager()->ClearBadgeForTesting(kAppId, &test_recorder);
  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kClearBadge);
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());
  EXPECT_EQ(kAppId, delegate()->cleared_badges().front());
}

TEST_F(BadgeManagerUnittest, BadgingMultipleProfiles) {
  std::unique_ptr<Profile> other_profile = std::make_unique<TestingProfile>();
  std::vector<web_app::AppId> other_updated_apps;
  StartTestWebAppProvider(other_profile.get(), other_updated_apps);
  auto* other_badge_manager =
      BadgeManagerFactory::GetInstance()->GetForProfile(other_profile.get());

  auto owned_other_delegate = std::make_unique<TestBadgeManagerDelegate>(
      other_profile.get(), other_badge_manager);
  auto* other_delegate = owned_other_delegate.get();
  other_badge_manager->SetDelegate(std::move(owned_other_delegate));

  other_badge_manager->SetBadgeForTesting(kAppId, base::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(kAppId, base::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->ClearBadgeForTesting(kAppId,
                                            ukm::TestUkmRecorder::Get());

  badge_manager()->ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());

  EXPECT_EQ(3UL, other_delegate->set_badges().size());
  EXPECT_EQ(0UL, delegate()->set_badges().size());

  EXPECT_EQ(1UL, other_delegate->cleared_badges().size());
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());

  EXPECT_EQ(kAppId, other_delegate->set_badges().back().first);
  EXPECT_EQ(base::nullopt, other_delegate->set_badges().back().second);

  EXPECT_EQ(1UL, updated_apps().size());
  EXPECT_EQ(kAppId, updated_apps()[0]);

  // TestBadgingAppRegistryController neglects to update last badging time, so
  // every update is recorded.
  EXPECT_FALSE(other_updated_apps.empty());
  EXPECT_EQ(kAppId, other_updated_apps[0]);
}

// Tests methods which call into the badge manager delegate do not crash when
// the delegate is unset.
TEST_F(BadgeManagerUnittest, BadgingWithNoDelegateDoesNotCrash) {
  badge_manager()->SetDelegate(nullptr);

  badge_manager()->SetBadgeForTesting(kAppId, base::nullopt,
                                      ukm::TestUkmRecorder::Get());
  badge_manager()->SetBadgeForTesting(
      kAppId, base::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager()->ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
}

}  // namespace badging
