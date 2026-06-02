// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_TEST_UPDATE_CLIENT_EVENT_WAITER_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_TEST_UPDATE_CLIENT_EVENT_WAITER_H_

#include <string>

#include "base/run_loop.h"
#include "components/update_client/update_client.h"

namespace extensions {

// Waits for completion events from UpdateClient. Must be named "test" because
// it contains a base::RunLoop, which is only allowed in test code in //chrome.
class TestUpdateClientEventWaiter
    : public update_client::UpdateClient::Observer {
 public:
  explicit TestUpdateClientEventWaiter(const std::string& id);

  TestUpdateClientEventWaiter(const TestUpdateClientEventWaiter&) = delete;
  TestUpdateClientEventWaiter& operator=(const TestUpdateClientEventWaiter&) =
      delete;

  ~TestUpdateClientEventWaiter() override;

  // update_client::UpdateClient::Observer:
  void OnEvent(const update_client::CrxUpdateItem& item) final;

  update_client::ComponentState Wait();

 private:
  const std::string id_;
  update_client::ComponentState state_ =
      update_client::ComponentState::kUpdateError;
  base::RunLoop run_loop_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_TEST_UPDATE_CLIENT_EVENT_WAITER_H_
