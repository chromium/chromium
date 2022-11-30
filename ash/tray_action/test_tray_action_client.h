// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_
#define ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_

#include <vector>

#include "ash/public/mojom/tray_action.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class TestTrayActionClient : public mojom::TrayActionClient {
 public:
  TestTrayActionClient();

  TestTrayActionClient(const TestTrayActionClient&) = delete;
  TestTrayActionClient& operator=(const TestTrayActionClient&) = delete;

  ~TestTrayActionClient() override;

  mojo::PendingRemote<mojom::TrayActionClient> CreateRemoteAndBind();

  void ClearRecordedRequests();

  const std::vector<mojom::LockScreenNoteOrigin>& note_origins() const {
    return note_origins_;
  }

  const std::vector<mojom::CloseLockScreenNoteReason>& close_note_reasons()
      const {
    return close_note_reasons_;
  }

  // mojom::TrayActionClient:
  void RequestNewLockScreenNote(mojom::LockScreenNoteOrigin origin) override;
  void CloseLockScreenNote(mojom::CloseLockScreenNoteReason reason) override;

 private:
  mojo::Receiver<mojom::TrayActionClient> receiver_{this};

  std::vector<mojom::LockScreenNoteOrigin> note_origins_;
  std::vector<mojom::CloseLockScreenNoteReason> close_note_reasons_;
};

}  // namespace ash

#endif  // ASH_TRAY_ACTION_TEST_TRAY_ACTION_CLIENT_H_
