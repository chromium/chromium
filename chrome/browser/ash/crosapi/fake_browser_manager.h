// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_

#include "base/values.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace crosapi {

// A fake implementation of BrowserManager, used for testing.
class FakeBrowserManager : public BrowserManager {
 public:
  FakeBrowserManager();
  FakeBrowserManager(const FakeBrowserManager&) = delete;
  FakeBrowserManager& operator=(const FakeBrowserManager&) = delete;

  ~FakeBrowserManager() override;

  void set_new_fullscreen_window_creation_result(mojom::CreationResult result) {
    new_fullscreen_window_creation_result_ = result;
  }

  void set_wait_for_mojo_disconnect(bool value) {
    wait_for_mojo_disconnect_ = value;
  }

  // Set up response data to be sent for the callback of Fetch.
  void SetGetFeedbackDataResponse(base::Value::Dict response);

  // Simulates crosapi mojo disconnection event observed.
  void SignalMojoDisconnected();

  // Sets the state of `BrowserManager` to `Running`, and triggers all
  // registered observers.
  void StartRunning();

  // Sets the state of `BrowserManager` to `Stopped`, and triggers all
  // registered observers.
  void StopRunning();

  // BrowserManager:
  void NewFullscreenWindow(const GURL& url,
                           NewFullscreenWindowCallback callback) override;
  void GetFeedbackData(GetFeedbackDataCallback callback) override;
  void InitializeAndStartIfNeeded() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

 private:
  // If this flag is set to true, simulate the case that mojo disconnect
  // signal is received before the log data is fetched.
  bool wait_for_mojo_disconnect_ = false;

  // The creation result to be returned by `NewFullscreenWindow`.
  mojom::CreationResult new_fullscreen_window_creation_result_ =
      mojom::CreationResult::kUnknown;

  // Stores the response to be sent back for GetFeedbackData callback.
  base::Value::Dict feedback_response_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FAKE_BROWSER_MANAGER_H_
