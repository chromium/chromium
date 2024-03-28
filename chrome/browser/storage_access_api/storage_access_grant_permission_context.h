// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_context_base.h"
#include "net/first_party_sets/first_party_set_metadata.h"

class GURL;

namespace permissions {
class PermissionRequestID;
}

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
    : public permissions::PermissionContextBase {
 public:
  explicit StorageAccessGrantPermissionContext(
      content::BrowserContext* browser_context);

  StorageAccessGrantPermissionContext(
      const StorageAccessGrantPermissionContext&) = delete;
  StorageAccessGrantPermissionContext& operator=(
      const StorageAccessGrantPermissionContext&) = delete;

  ~StorageAccessGrantPermissionContext() override;

  // Exposes `DecidePermission` for tests.
  void DecidePermissionForTesting(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback);

  static int GetImplicitGrantLimitForTesting();
  static void SetImplicitGrantLimitForTesting(int limit);

 private:
  // PermissionContextBase:
  void DecidePermission(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback) override;
  ContentSetting GetPermissionStatusInternal(
      content::RenderFrameHost* render_frame_host,
      const GURL& requesting_origin,
      const GURL& embedding_origin) const override;
  void NotifyPermissionSet(const permissions::PermissionRequestID& id,
                           const GURL& requesting_origin,
                           const GURL& embedding_origin,
                           permissions::BrowserPermissionCallback callback,
                           bool persist,
                           ContentSetting content_setting,
                           bool is_one_time,
                           bool is_final_decision) override;
  void UpdateContentSetting(const GURL& requesting_origin,
                            const GURL& embedding_origin,
                            ContentSetting content_setting,
                            bool is_one_time) override;

  // Internal implementation for NotifyPermissionSet.
  void NotifyPermissionSetInternal(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      permissions::BrowserPermissionCallback callback,
      bool persist,
      ContentSetting content_setting,
      RequestOutcome outcome);

  // Checks First-Party Sets metadata to determine if auto-grants or
  // auto-denials are applicable. If no autogrant or autodenial is applicable,
  // this tries to to use an implicit grant, and finally may prompt the user if
  // necessary.
  void CheckForAutoGrantOrAutoDenial(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback,
      net::FirstPartySetMetadata metadata);

  // Determines whether the top-level user-interaction heuristic was satisfied,
  // and if so, prompts the user.
  void OnCheckedUserInteractionHeuristic(
      permissions::PermissionRequestData request_data,
      permissions::BrowserPermissionCallback callback,
      bool had_top_level_user_interaction);

  base::WeakPtrFactory<StorageAccessGrantPermissionContext> weak_factory_{this};
};

#endif  // CHROME_BROWSER_STORAGE_ACCESS_API_STORAGE_ACCESS_GRANT_PERMISSION_CONTEXT_H_
