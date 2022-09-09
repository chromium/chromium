// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

#include <vector>

SingleClientStatusChangeChecker::SingleClientStatusChangeChecker(
    syncer::SyncServiceImpl* service)
    : MultiClientStatusChangeChecker({service}) {}

SingleClientStatusChangeChecker::~SingleClientStatusChangeChecker() = default;

syncer::SyncServiceImpl* SingleClientStatusChangeChecker::service() {
  return services()[0];
}
