// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fake_browser_manager.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockCrOSComponentManager
    : public component_updater::CrOSComponentManager {
 public:
  MockCrOSComponentManager() = default;

  MockCrOSComponentManager(const MockCrOSComponentManager&) = delete;
  MockCrOSComponentManager& operator=(const MockCrOSComponentManager&) = delete;

  MOCK_METHOD1(SetDelegate, void(Delegate* delegate));
  MOCK_METHOD4(Load,
               void(const std::string& name,
                    MountPolicy mount_policy,
                    UpdatePolicy update_policy,
                    LoadCallback load_callback));
  MOCK_METHOD1(Unload, bool(const std::string& name));
  MOCK_METHOD2(RegisterCompatiblePath,
               void(const std::string& name, const base::FilePath& path));
  MOCK_METHOD1(UnregisterCompatiblePath, void(const std::string& name));
  MOCK_CONST_METHOD1(GetCompatiblePath,
                     base::FilePath(const std::string& name));
  MOCK_METHOD1(IsRegisteredMayBlock, bool(const std::string& name));
  MOCK_METHOD0(RegisterInstalled, void());

 protected:
  ~MockCrOSComponentManager() override = default;
};

}  // namespace

namespace crosapi {

FakeBrowserManager::FakeBrowserManager()
    : BrowserManager(base::MakeRefCounted<MockCrOSComponentManager>()) {}

FakeBrowserManager::~FakeBrowserManager() = default;

void FakeBrowserManager::SetGetFeedbackDataResponse(
    base::Value::Dict response) {
  feedback_response_ = std::move(response);
}

void FakeBrowserManager::SignalMojoDisconnected() {
  SetState(State::TERMINATING);
}

void FakeBrowserManager::StartRunning() {
  SetState(State::RUNNING);
}

bool FakeBrowserManager::IsRunning() const {
  return is_running_;
}

bool FakeBrowserManager::IsRunningOrWillRun() const {
  return is_running_;
}

void FakeBrowserManager::NewFullscreenWindow(
    const GURL& url,
    BrowserManager::NewFullscreenWindowCallback callback) {
  std::move(callback).Run(new_fullscreen_window_creation_result_);
}

void FakeBrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  // Run |callback| with the pre-set |feedback_responses_|, unless testing
  // client requests waiting for crosapi mojo disconnected event being observed.
  if (!wait_for_mojo_disconnect_)
    std::move(callback).Run(std::move(feedback_response_));
}

void FakeBrowserManager::OnSessionStateChanged() {}

}  // namespace crosapi
