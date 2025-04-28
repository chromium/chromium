// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_
#define CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_

#include "base/time/time.h"

namespace glic {

// These enums are persisted in the disk as integers. They should not be
// renumbered or removed.
enum UserStatusCode {
  ENABLED = 0,
  DISABLED_BY_ADMIN = 1,
  DISABLED_OTHER = 2,
  SERVER_UNAVAILABLE = 3,
};

struct CachedUserStatus {
  UserStatusCode user_status_code;
  base::Time last_updated;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_USER_STATUS_CODE_H_
