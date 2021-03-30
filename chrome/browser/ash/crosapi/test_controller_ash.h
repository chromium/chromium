// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_

#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// This class is the ash-chrome implementation of the TestController interface.
// This class must only be used from the main thread.
class TestControllerAsh : public mojom::TestController {
 public:
  TestControllerAsh();
  TestControllerAsh(const TestControllerAsh&) = delete;
  TestControllerAsh& operator=(const TestControllerAsh&) = delete;
  ~TestControllerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::TestController> receiver);

  // crosapi::mojom::TestController:
  void DoesWindowExist(const std::string& window_id,
                       DoesWindowExistCallback callback) override;
  void ClickWindow(const std::string& window_id) override;
  void EnterOverviewMode(EnterOverviewModeCallback callback) override;
  void ExitOverviewMode(ExitOverviewModeCallback callback) override;
  void EnterTabletMode(EnterTabletModeCallback callback) override;
  void ExitTabletMode(ExitTabletModeCallback callback) override;
  void SendTouchEvent(const std::string& window_id,
                      mojom::TouchEventType type,
                      uint8_t pointer_id,
                      const gfx::PointF& location_in_window,
                      SendTouchEventCallback cb) override;
  void GetWindowPositionInScreen(const std::string& window_id,
                                 GetWindowPositionInScreenCallback cb) override;

 private:
  class OverviewWaiter;

  // Called when a waiter has finished waiting for its event.
  void WaiterFinished(OverviewWaiter* waiter);

  // Each call to EnterOverviewMode or ExitOverviewMode spawns a waiter for the
  // corresponding event. The waiters are stored in this struct and deleted once
  // the event triggers.
  std::vector<std::unique_ptr<OverviewWaiter>> overview_waiters_;

  // This class supports any number of connections. This allows multiple
  // crosapi clients.
  mojo::ReceiverSet<mojom::TestController> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_TEST_CONTROLLER_ASH_H_
