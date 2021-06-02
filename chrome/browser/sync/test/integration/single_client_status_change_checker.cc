// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

#include <vector>

SingleClientStatusChangeChecker::SingleClientStatusChangeChecker(
    syncer::ProfileSyncService* service)
    : MultiClientStatusChangeChecker({service}) {}

SingleClientStatusChangeChecker::~SingleClientStatusChangeChecker() {}

syncer::ProfileSyncService* SingleClientStatusChangeChecker::service() {
  return services()[0];
}
