// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"

#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"

namespace fake_server {

FakeServerMatchStatusChecker::FakeServerMatchStatusChecker()
    : fake_server_(sync_datatype_helper::test()->GetFakeServer()) {
  DCHECK(fake_server_);
  fake_server_->AddObserver(this);
}

FakeServerMatchStatusChecker::~FakeServerMatchStatusChecker() {
  fake_server_->RemoveObserver(this);
}

void FakeServerMatchStatusChecker::OnCommit(
    syncer::DataTypeSet committed_data_types) {
  CheckExitCondition();
}

void FakeServerMatchStatusChecker::OnSuccessfulGetUpdates() {
  CheckExitCondition();
}

fake_server::FakeServer* FakeServerMatchStatusChecker::fake_server() const {
  return fake_server_;
}

}  // namespace fake_server
