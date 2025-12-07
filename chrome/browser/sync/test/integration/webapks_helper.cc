// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/webapks_helper.h"

#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace webapks_helper {

namespace {

std::vector<sync_pb::WebApkSpecifics> SyncEntitiesToWebApkSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::WebApkSpecifics> web_apk;
  for (sync_pb::SyncEntity& entity : entities) {
    CHECK(entity.specifics().has_web_apk());
    web_apk.push_back(std::move(entity.specifics().web_apk()));
  }
  return web_apk;
}

webapk::WebApkSyncService* GetWebApkSyncServiceFromClient(int index) {
  return webapk::WebApkSyncServiceFactory::GetForProfile(
      sync_datatype_helper::test()->GetProfile(index));
}

const webapk::Registry& GetLocalWebApks(int index) {
  return GetWebApkSyncServiceFromClient(index)->GetRegistryForTesting();
}

}  // namespace

bool AreIconInfosMatching(const sync_pb::WebApkIconInfo& icon_info_1,
                          const sync_pb::WebApkIconInfo& icon_info_2) {
  return icon_info_1.size_in_px() == icon_info_2.size_in_px() &&
         icon_info_1.url() == icon_info_2.url() &&
         icon_info_1.purpose() == icon_info_2.purpose();
}

ServerWebApkMatchChecker::ServerWebApkMatchChecker(const Matcher& matcher)
    : matcher_(matcher) {}

ServerWebApkMatchChecker::~ServerWebApkMatchChecker() = default;

void ServerWebApkMatchChecker::OnCommit(
    syncer::DataTypeSet committed_data_types) {
  if (committed_data_types.Has(syncer::WEB_APKS)) {
    CheckExitCondition();
  }
}

bool ServerWebApkMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::WebApkSpecifics> entities =
      SyncEntitiesToWebApkSpecifics(
          fake_server()->GetSyncEntitiesByDataType(syncer::WEB_APKS));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

LocalWebApkMatchChecker::LocalWebApkMatchChecker(
    int profile_index,
    syncer::SyncServiceImpl* service,
    const Matcher& matcher)
    : SingleClientStatusChangeChecker(service),
      profile_index_(profile_index),
      matcher_(matcher) {}

LocalWebApkMatchChecker::~LocalWebApkMatchChecker() = default;

bool LocalWebApkMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  const webapk::Registry& registry = GetLocalWebApks(profile_index_);
  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, registry, &result_listener);
  *os << result_listener.str();
  return matches;
}

void LocalWebApkMatchChecker::OnSyncCycleCompleted(syncer::SyncService* sync) {
  CheckExitCondition();
}

}  // namespace webapks_helper
