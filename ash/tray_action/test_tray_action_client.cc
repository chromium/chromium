// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/tray_action/test_tray_action_client.h"

#include "ash/public/mojom/tray_action.mojom.h"

namespace ash {

TestTrayActionClient::TestTrayActionClient() = default;

TestTrayActionClient::~TestTrayActionClient() = default;

void TestTrayActionClient::ClearRecordedRequests() {
  note_origins_.clear();
  close_note_reasons_.clear();
}

void TestTrayActionClient::RequestNewLockScreenNote(
    mojom::LockScreenNoteOrigin origin) {
  note_origins_.push_back(origin);
}

void TestTrayActionClient::CloseLockScreenNote(
    mojom::CloseLockScreenNoteReason reason) {
  close_note_reasons_.push_back(reason);
}

mojo::PendingRemote<mojom::TrayActionClient>
TestTrayActionClient::CreateRemoteAndBind() {
  mojo::PendingRemote<mojom::TrayActionClient> remote;
  receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

}  // namespace ash
