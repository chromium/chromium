// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/permissions/content_setting_permission_context_base.h"
#include "net/first_party_sets/first_party_set_metadata.h"

class GURL;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Update StorageAccessAPIRequestOutcome in enums.xml when you add entries.
enum class RequestOutcome {
  // The request was granted because the requesting site and the top level site
  // were in the same First-Party Set.
  kGrantedByFirstPartySet = 0,
  // The request was granted because the requesting site had not yet used up its
  // allowance of implicit grants (`kStorageAccessAPIImplicitGrantLimit`).
  kGrantedByAllowance = 1,
  // The request was granted by the user.
  kGrantedByUser = 2,
  // The request was denied because the requesting site and the top level site
  // were not in the same First-Party Set.
  // This variant is no longer used.
  // kDeniedByFirstPartySet = 3,

  // The request was denied by the user.
  kDeniedByUser = 4,
  // The request was denied because it lacked user gesture, or one of the
  // domains was invalid, or the feature was disabled.
  kDeniedByPrerequisites = 5,
  // The request was dismissed by the user.
  kDismissedByUser = 6,
  // The user has already been asked and made a choice (and was not asked
  // again).
  kReusedPreviousDecision = 7,
  // The request was denied because the most recent top-level interaction on
  // the embedded site was too long ago, or there is no such interaction.
  kDeniedByTopLevelInteractionHeuristic = 8,
  // 3p cookies are already allowed by user agent, so there is no need to ask.
  kAllowedByCookieSettings = 9,
  // The permission was previously granted without user action. E.g. through
  // FPS.
  kReusedImplicitGrant = 10,
  // 3p cookies are blocked by user explicitly, so there is no need to ask.
  kDeniedByCookieSettings = 11,
  // The requesting origin is same-site with the embedding origin.
  kAllowedBySameSite = 12,
  // The request was aborted by the browser (e.g. because the RenderFrameHost
  // was deleted).
  kDeniedAborted = 13,
  // The request was allowed by SAA/FedCM integration.
  kAllowedByFedCM = 14,

  kMaxValue = kAllowedByFedCM,
};

class StorageAccessGrantPermissionContext
    : public permissions::ContentSettingPermissionContextBase {
 public:
  using PassKey = base::PassKey<StorageAccessGrantPermissionContext>;

  explicit StorageAccessGrantPermissionContext(
      content::BrowserContext* browser_context);

  StorageAccessGrantPermissionContext(
      const StorageAccessGrantPermissionContext&) = delete;
  StorageAccessGrantPermissionContext& operator=(
      const StorageAccessGrantPermissionContext&) = delete;

  ~StorageAccessGrantPermissionContext() override;

  // Exposes `RequestPermission` for tests.
  void RequestPermissionForTesting(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback);

  // Exposes `DecidePermission` for tests.
  void DecidePermissionForTesting(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback);

  static int GetImplicitGrantLimitForTesting();
  static void SetImplicitGrantLimitForTesting(int limit);

 private:
  // PermissionContextBase:
  void RequestPermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback) override;
  void DecidePermission(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback) override;
  void NotifyPermissionSet(
      const permissions::PermissionRequestData& request_data,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      PermissionDecision decision,
      bool is_final_decision) override;

  // ContentSettingPermissionContextBase
  ContentSetting GetContentSettingStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void UpdateContentSetting(
      const permissions::PermissionRequestData& request_data,
      ContentSetting content_setting,
      bool is_one_time) override;

  // Internal implementation for NotifyPermissionSet.
  void NotifyPermissionSetInternal(
      const permissions::PermissionRequestData& request_data,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      PermissionDecision decision,
      RequestOutcome outcome);

  // Checks First-Party Sets metadata to determine if auto-grants or
  // auto-denials are applicable. If no autogrant or autodenial is applicable,
  // this tries to to use an implicit grant, and finally may prompt the user if
  // necessary.
  void CheckForAutoGrantOrAutoDenial(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback,
      net::FirstPartySetMetadata metadata);

  // Determines whether the top-level user-interaction heuristic was satisfied,
  // and if so, prompts the user.
  void OnCheckedUserInteractionHeuristic(
      std::unique_ptr<permissions::PermissionRequestData> request_data,
      permissions::BrowserPermissionCallback callback,
      bool had_top_level_user_interaction);

  void ReportRelatedWebsiteSetsDeprecation(content::RenderFrameHost* rfh);

  base::WeakPtrFactory<StorageAccessGrantPermissionContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
