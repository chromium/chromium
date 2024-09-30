// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_context.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::InSequence;
using testing::StrictMock;

namespace {

constexpr char kDummyReader[] = "dummy reader";
constexpr char kDummyReader2[] = "dummy reader 2";

class FakeOneTimePermissionsTracker : public OneTimePermissionsTracker {
 public:
  using OneTimePermissionsTracker::NotifyLastPageFromOriginClosed;
};

class FakeSmartCardReaderTracker : public SmartCardReaderTracker {
 public:
  void Start(Observer* observer, StartCallback callback) override {
    observers_.AddObserverIfMissing(observer);

    std::vector<ReaderInfo> reader_list;
    reader_list.reserve(info_map_.size());

    for (const auto& [_, info] : info_map_) {
      reader_list.push_back(info);
    }

    std::move(callback).Run(std::move(reader_list));
  }

  void Stop(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void AddReader(ReaderInfo info) {
    CHECK(!info_map_.contains(info.name));
    std::string name = info.name;
    info_map_.emplace(std::move(name), std::move(info));
  }

  void AddReaderWithCard(const std::string& name) {
    ReaderInfo info;
    info.name = name;
    info.present = true;
    info.event_count = 1;
    AddReader(std::move(info));
  }

  void RemoveCard(const std::string& reader_name) {
    auto it = info_map_.find(reader_name);
    CHECK(it != info_map_.end());

    ReaderInfo& info = it->second;

    CHECK(info.present);
    CHECK(!info.empty);

    info.present = false;
    info.empty = true;
    ++info.event_count;

    observers_.NotifyReaderChanged(info);
  }

  void RemoveReader(const std::string& reader_name) {
    auto it = info_map_.find(reader_name);
    CHECK(it != info_map_.end());

    info_map_.erase(it);

    observers_.NotifyReaderRemoved(reader_name);
  }

  bool HasObservers() const { return !observers_.empty(); }

 private:
  ObserverList observers_;
  std::map<std::string, ReaderInfo> info_map_;
};

}  // namespace

class SmartCardPermissionContextTest : public testing::Test {
 protected:
  void SetUp() override {
    SmartCardReaderTrackerFactory::GetInstance()->SetTestingFactory(
        &profile_,
        base::BindRepeating(
            &SmartCardPermissionContextTest::CreateFakeSmartCardReaderTracker,
            base::Unretained(this)));

    OneTimePermissionsTrackerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&SmartCardPermissionContextTest::
                                           CreateFakeOneTimePermissionsTracker,
                                       base::Unretained(this)));
  }

  bool HasReaderPermission(SmartCardPermissionContext& context,
                           const url::Origin& origin,
                           const std::string& reader_name) {
    return context.HasReaderPermission(origin, reader_name);
  }

  void GrantEphemeralReaderPermission(SmartCardPermissionContext& context,
                                      const url::Origin& origin,
                                      const std::string& reader_name) {
    return context.GrantEphemeralReaderPermission(origin, reader_name);
  }

  void GrantPersistentReaderPermission(SmartCardPermissionContext& context,
                                       const url::Origin& origin,
                                       const std::string& reader_name) {
    return context.GrantPersistentReaderPermission(origin, reader_name);
  }

  std::unique_ptr<KeyedService> CreateFakeSmartCardReaderTracker(
      content::BrowserContext* context) {
    CHECK_EQ(context, &profile_);

    return std::make_unique<StrictMock<FakeSmartCardReaderTracker>>();
  }

  std::unique_ptr<KeyedService> CreateFakeOneTimePermissionsTracker(
      content::BrowserContext* context) {
    CHECK_EQ(context, &profile_);

    return std::make_unique<FakeOneTimePermissionsTracker>();
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SmartCardPermissionContextTest, GrantEphemeralReaderPermission) {
  auto& reader_tracker = static_cast<FakeSmartCardReaderTracker&>(
      SmartCardReaderTrackerFactory::GetForProfile(profile_));

  reader_tracker.AddReaderWithCard(kDummyReader);

  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_FALSE(reader_tracker.HasObservers());

  GrantEphemeralReaderPermission(permission_context, foo_origin, kDummyReader);

  EXPECT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_TRUE(reader_tracker.HasObservers());
}

TEST_F(SmartCardPermissionContextTest,
       RevokeEphemeralPermissionWhenCardRemoved) {
  auto& reader_tracker = static_cast<FakeSmartCardReaderTracker&>(
      SmartCardReaderTrackerFactory::GetForProfile(profile_));
  reader_tracker.AddReaderWithCard(kDummyReader);

  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantEphemeralReaderPermission(permission_context, foo_origin, kDummyReader);

  ASSERT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  ASSERT_TRUE(reader_tracker.HasObservers());

  reader_tracker.RemoveCard(kDummyReader);

  // The ephemeral permission should have been revoked.
  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_FALSE(reader_tracker.HasObservers());
}

TEST_F(SmartCardPermissionContextTest,
       RevokeEphemeralPermissionWhenReaderRemoved) {
  auto& reader_tracker = static_cast<FakeSmartCardReaderTracker&>(
      SmartCardReaderTrackerFactory::GetForProfile(profile_));
  reader_tracker.AddReaderWithCard(kDummyReader);

  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantEphemeralReaderPermission(permission_context, foo_origin, kDummyReader);

  ASSERT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  ASSERT_TRUE(reader_tracker.HasObservers());

  reader_tracker.RemoveReader(kDummyReader);

  // The ephemeral permission should have been revoked.
  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_FALSE(reader_tracker.HasObservers());
}

TEST_F(SmartCardPermissionContextTest, RevokeEphemeralPermissionWhenAppClosed) {
  auto* one_time_tracker = static_cast<FakeOneTimePermissionsTracker*>(
      OneTimePermissionsTrackerFactory::GetForBrowserContext(&profile_));

  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantEphemeralReaderPermission(permission_context, foo_origin, kDummyReader);

  ASSERT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));

  one_time_tracker->NotifyLastPageFromOriginClosed(foo_origin);

  // The ephemeral permission should have been revoked.
  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
}

// Covers callers of this method such as the PowerSuspendObserver.
TEST_F(SmartCardPermissionContextTest, RevokeEphemeralPermissions) {
  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantEphemeralReaderPermission(permission_context, foo_origin, kDummyReader);

  ASSERT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));

  permission_context.RevokeEphemeralPermissions();

  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
}

TEST_F(SmartCardPermissionContextTest, Blocked) {
  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantPersistentReaderPermission(permission_context, foo_origin, kDummyReader);

  EXPECT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));

  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(&profile_);
  settings_map->SetContentSettingDefaultScope(
      foo_origin.GetURL(), GURL(), ContentSettingsType::SMART_CARD_GUARD,
      ContentSetting::CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));

  settings_map->SetContentSettingDefaultScope(
      foo_origin.GetURL(), GURL(), ContentSettingsType::SMART_CARD_GUARD,
      ContentSetting::CONTENT_SETTING_DEFAULT);

  EXPECT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));

  permission_context.RevokeObjectPermissions(foo_origin);
}

TEST_F(SmartCardPermissionContextTest, RevokeAllPermissions) {
  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));
  auto bar_origin = url::Origin::Create(GURL("https://bar.com/"));
  auto cthulhu_origin = url::Origin::Create(GURL("https://cthulhu.rlyeh/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantPersistentReaderPermission(permission_context, foo_origin, kDummyReader);
  GrantEphemeralReaderPermission(permission_context, bar_origin, kDummyReader);
  GrantPersistentReaderPermission(permission_context, cthulhu_origin,
                                  kDummyReader2);

  EXPECT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_TRUE(
      HasReaderPermission(permission_context, bar_origin, kDummyReader));
  EXPECT_TRUE(
      HasReaderPermission(permission_context, cthulhu_origin, kDummyReader2));

  permission_context.RevokeAllPermissions();

  // should reset permissions of all types
  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_FALSE(
      HasReaderPermission(permission_context, bar_origin, kDummyReader));
  EXPECT_FALSE(
      HasReaderPermission(permission_context, cthulhu_origin, kDummyReader2));
}

TEST_F(SmartCardPermissionContextTest, RevokePersistentPermission) {
  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));
  auto bar_origin = url::Origin::Create(GURL("https://bar.com/"));
  auto cthulhu_origin = url::Origin::Create(GURL("https://cthulhu.rlyeh/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantPersistentReaderPermission(permission_context, foo_origin, kDummyReader);
  GrantEphemeralReaderPermission(permission_context, bar_origin, kDummyReader);
  GrantPersistentReaderPermission(permission_context, cthulhu_origin,
                                  kDummyReader2);

  EXPECT_TRUE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_TRUE(
      HasReaderPermission(permission_context, bar_origin, kDummyReader));
  EXPECT_TRUE(
      HasReaderPermission(permission_context, cthulhu_origin, kDummyReader2));

  permission_context.RevokePersistentPermission(kDummyReader, foo_origin);
  permission_context.RevokePersistentPermission(kDummyReader, bar_origin);
  permission_context.RevokePersistentPermission(kDummyReader2, cthulhu_origin);

  // should reset permissions only of the persistent type
  EXPECT_FALSE(
      HasReaderPermission(permission_context, foo_origin, kDummyReader));
  EXPECT_TRUE(
      HasReaderPermission(permission_context, bar_origin, kDummyReader));
  EXPECT_FALSE(
      HasReaderPermission(permission_context, cthulhu_origin, kDummyReader2));

  permission_context.RevokeAllPermissions();
}

TEST_F(SmartCardPermissionContextTest, GetPersistentReaderGrants) {
  auto foo_origin = url::Origin::Create(GURL("https://foo.com/"));
  auto bar_origin = url::Origin::Create(GURL("https://bar.com/"));
  auto cthulhu_origin = url::Origin::Create(GURL("https://cthulhu.rlyeh/"));

  SmartCardPermissionContext permission_context(&profile_);

  GrantPersistentReaderPermission(permission_context, foo_origin, kDummyReader);
  GrantEphemeralReaderPermission(permission_context, bar_origin, kDummyReader);
  GrantPersistentReaderPermission(permission_context, cthulhu_origin,
                                  kDummyReader2);

  std::vector<SmartCardPermissionContext::ReaderGrants> grants =
      permission_context.GetPersistentReaderGrants();

  // should return only persistent grants
  ASSERT_THAT(grants,
              testing::ElementsAre(SmartCardPermissionContext::ReaderGrants(
                                       kDummyReader, {foo_origin}),
                                   SmartCardPermissionContext::ReaderGrants(
                                       kDummyReader2, {cthulhu_origin})));

  permission_context.RevokeAllPermissions();
}
