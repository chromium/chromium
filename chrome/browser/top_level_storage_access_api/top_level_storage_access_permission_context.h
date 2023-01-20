// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOP_LEVEL_STORAGE_ACCESS_API_TOP_LEVEL_STORAGE_ACCESS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_TOP_LEVEL_STORAGE_ACCESS_API_TOP_LEVEL_STORAGE_ACCESS_PERMISSION_CONTEXT_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/permissions/permission_context_base.h"
#include "net/first_party_sets/first_party_set_metadata.h"

extern const int kDefaultImplicitGrantLimit;

class GURL;

namespace permissions {
class PermissionRequestID;
}

// This enum temporarily copied from the storage_access_api equivalent. It will
// soon be modified and a separate metric will be written.
enum class CookieRequestOutcome {
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
  kDeniedByFirstPartySet = 3,
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
  kMaxValue = kReusedPreviousDecision,
};

class TopLevelStorageAccessPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit TopLevelStorageAccessPermissionContext(
      content::BrowserContext* browser_context);

  TopLevelStorageAccessPermissionContext(
      const TopLevelStorageAccessPermissionContext&) = delete;
  TopLevelStorageAccessPermissionContext& operator=(
      const TopLevelStorageAccessPermissionContext&) = delete;

  ~TopLevelStorageAccessPermissionContext() override;

  // Exposes `DecidePermission` for tests.
  void DecidePermissionForTesting(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback);

 private:
  // PermissionContextBase:
  void DecidePermission(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
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
      CookieRequestOutcome outcome);

  // Checks First-Party Sets metadata to determine whether the request should be
  // auto-rejected or auto-denied.
  void CheckForAutoGrantOrAutoDenial(
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback,
      net::FirstPartySetMetadata metadata);

  base::WeakPtrFactory<TopLevelStorageAccessPermissionContext> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_TOP_LEVEL_STORAGE_ACCESS_API_TOP_LEVEL_STORAGE_ACCESS_PERMISSION_CONTEXT_H_
