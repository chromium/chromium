// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_
#define CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_

#include "base/json/json_value_converter.h"
#include "base/time/time.h"

namespace glic {

// Keys of the pref dict.
inline constexpr char kUserStatus[] = "user_status";
inline constexpr char kUpdatedAt[] = "updated_at";
inline constexpr char kAccountId[] = "account_id";

// Keys of the JSON response of the glic user status RPC.
inline constexpr char kIsGlicEnabled[] = "isGlicEnabled";
inline constexpr char kIsAccessDeniedByAdmin[] = "isAccessDeniedByAdmin";
inline constexpr char kIsEnterpriseAccountDataProtected[] =
    "isEnterpriseAccountDataProtected";

// These enums are persisted in the disk as integers. They should not be
// renumbered or removed.
enum UserStatusCode {
  ENABLED = 0,
  DISABLED_BY_ADMIN = 1,
  DISABLED_OTHER = 2,
  SERVER_UNAVAILABLE = 3,
};

struct CachedUserStatus {
  UserStatusCode user_status_code = UserStatusCode::ENABLED;
  // If true, this is an enterprise account for whom different disclosures
  // should be shown. See b/413482904 for details.
  bool is_enterprise_account_data_protected = false;
  base::Time last_updated;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_
