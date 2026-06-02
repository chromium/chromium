// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/test_update_client_event_waiter.h"

#include <string>

#include "base/run_loop.h"
#include "components/update_client/crx_update_item.h"

namespace extensions {

TestUpdateClientEventWaiter::TestUpdateClientEventWaiter(const std::string& id)
    : id_(id) {}

TestUpdateClientEventWaiter::~TestUpdateClientEventWaiter() = default;

void TestUpdateClientEventWaiter::OnEvent(
    const update_client::CrxUpdateItem& item) {
  if (id_ == item.id &&
      (item.state == update_client::ComponentState::kUpdated ||
       item.state == update_client::ComponentState::kUpToDate ||
       item.state == update_client::ComponentState::kUpdateError)) {
    state_ = item.state;
    run_loop_.Quit();
  }
}

update_client::ComponentState TestUpdateClientEventWaiter::Wait() {
  run_loop_.Run();
  return state_;
}

}  // namespace extensions
