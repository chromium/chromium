// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/themes_helper.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"

namespace {

using themes_helper::GetCustomTheme;
using themes_helper::IsSystemThemeDistinctFromDefaultTheme;
using themes_helper::UseCustomTheme;
using themes_helper::UseDefaultTheme;
using themes_helper::UseSystemTheme;
using themes_helper::UsingCustomTheme;
using themes_helper::UsingDefaultTheme;
using themes_helper::UsingSystemTheme;

// Note: All of these matchers take a sync_pb::ThemeSpecifics.

MATCHER(HasDefaultTheme, "") {
  return !arg.use_custom_theme() && !arg.use_system_theme_by_default() &&
         !arg.has_custom_theme_id();
}

MATCHER(HasSystemTheme, "") {
  return !arg.use_custom_theme() && arg.use_system_theme_by_default() &&
         !arg.has_custom_theme_id();
}

MATCHER_P(HasCustomThemeWithId, theme_id, "") {
  return arg.use_custom_theme() && arg.custom_theme_id() == theme_id;
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateDefaultThemeEntity() {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_theme();
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      ThemeSyncableService::kSyncEntityClientTag,
      ThemeSyncableService::kSyncEntityTitle, specifics,
      /*creation_time=*/0, /*last_modified_time=*/0);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateSystemThemeEntity() {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_theme()->set_use_system_theme_by_default(true);
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      ThemeSyncableService::kSyncEntityClientTag,
      ThemeSyncableService::kSyncEntityTitle, specifics,
      /*creation_time=*/0, /*last_modified_time=*/0);
}

std::unique_ptr<syncer::LoopbackServerEntity> CreateCustomThemeEntity(
    const std::string& theme_id) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_theme()->set_use_custom_theme(true);
  specifics.mutable_theme()->set_custom_theme_id(theme_id);
  specifics.mutable_theme()->set_custom_theme_name("custom theme");
  return syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
      ThemeSyncableService::kSyncEntityClientTag,
      ThemeSyncableService::kSyncEntityTitle, specifics,
      /*creation_time=*/0, /*last_modified_time=*/0);
}

// A helper class that waits for the (single) THEME entity on the FakeServer to
// match a given matcher.
class ServerThemeMatchChecker
    : public fake_server::FakeServerMatchStatusChecker {
 public:
  using Matcher = testing::Matcher<sync_pb::ThemeSpecifics>;

  explicit ServerThemeMatchChecker(const Matcher& matcher);
  ~ServerThemeMatchChecker() override;
  ServerThemeMatchChecker(const ServerThemeMatchChecker&) = delete;
  ServerThemeMatchChecker& operator=(const ServerThemeMatchChecker&) = delete;

  // FakeServer::Observer overrides.
  void OnCommit(const std::string& committer_invalidator_client_id,
                syncer::ModelTypeSet committed_model_types) override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const Matcher matcher_;
};

ServerThemeMatchChecker::ServerThemeMatchChecker(const Matcher& matcher)
    : matcher_(matcher) {}

ServerThemeMatchChecker::~ServerThemeMatchChecker() = default;

void ServerThemeMatchChecker::OnCommit(
    const std::string& committer_invalidator_client_id,
    syncer::ModelTypeSet committed_model_types) {
  if (committed_model_types.Has(syncer::THEMES)) {
    CheckExitCondition();
  }
}

bool ServerThemeMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> entities =
      fake_server()->GetSyncEntitiesByModelType(syncer::THEMES);

  if (entities.empty()) {
    return false;
  }
  DCHECK_EQ(entities.size(), 1u);
  DCHECK(entities[0].specifics().has_theme());

  testing::StringMatchResultListener result_listener;
  const bool matches = testing::ExplainMatchResult(
      matcher_, entities[0].specifics().theme(), &result_listener);
  *os << result_listener.str();
  return matches;
}

class SingleClientThemesSyncTest : public SyncTest {
 public:
  SingleClientThemesSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientThemesSyncTest() override = default;

  bool TestUsesSelfNotifications() override { return false; }
};

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, UploadsThemesOnInstall) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  ASSERT_FALSE(UsingCustomTheme(GetProfile(0)));
  UseCustomTheme(GetProfile(0), 0);
  EXPECT_TRUE(
      ServerThemeMatchChecker(HasCustomThemeWithId(GetCustomTheme(0))).Wait());

  if (IsSystemThemeDistinctFromDefaultTheme(GetProfile(0))) {
    ASSERT_FALSE(UsingSystemTheme(GetProfile(0)));
    UseSystemTheme(GetProfile(0));
    EXPECT_TRUE(ServerThemeMatchChecker(HasSystemTheme()).Wait());
  }

  ASSERT_FALSE(UsingDefaultTheme(GetProfile(0)));
  UseDefaultTheme(GetProfile(0));
  EXPECT_TRUE(ServerThemeMatchChecker(HasDefaultTheme()).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, UploadsPreexistingTheme) {
  ASSERT_TRUE(SetupClients());

  UseCustomTheme(GetProfile(0), 0);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  EXPECT_TRUE(
      ServerThemeMatchChecker(HasCustomThemeWithId(GetCustomTheme(0))).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DownloadsCustomTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  GetFakeServer()->InjectEntity(CreateCustomThemeEntity(GetCustomTheme(0)));
  // Note: The custom theme won't actually get installed; just check that it's
  // pending for installation.
  EXPECT_TRUE(
      ThemePendingInstallChecker(GetProfile(0), GetCustomTheme(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DownloadsSystemTheme) {
  // Skip if this platform doesn't distinguish between system and default theme.
  ASSERT_TRUE(SetupClients());
  if (!IsSystemThemeDistinctFromDefaultTheme(GetProfile(0))) {
    return;
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Set up a custom theme first, so we can then switch back to system.
  UseCustomTheme(GetProfile(0), 0);
  ASSERT_TRUE(
      ServerThemeMatchChecker(HasCustomThemeWithId(GetCustomTheme(0))).Wait());
  ASSERT_TRUE(UsingCustomTheme(GetProfile(0)));

  ASSERT_FALSE(UsingSystemTheme(GetProfile(0)));
  GetFakeServer()->InjectEntity(CreateSystemThemeEntity());
  EXPECT_TRUE(SystemThemeChecker(GetProfile(0)).Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientThemesSyncTest, DownloadsDefaultTheme) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  // Set up a custom theme first, so we can then switch back to default.
  UseCustomTheme(GetProfile(0), 0);
  ASSERT_TRUE(
      ServerThemeMatchChecker(HasCustomThemeWithId(GetCustomTheme(0))).Wait());
  ASSERT_TRUE(UsingCustomTheme(GetProfile(0)));

  ASSERT_FALSE(UsingDefaultTheme(GetProfile(0)));
  GetFakeServer()->InjectEntity(CreateDefaultThemeEntity());
  EXPECT_TRUE(DefaultThemeChecker(GetProfile(0)).Wait());
}

}  // namespace
