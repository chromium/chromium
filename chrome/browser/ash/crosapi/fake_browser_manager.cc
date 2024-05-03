// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fake_browser_manager.h"

#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/component_updater/ash/component_manager_ash.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

class MockComponentManagerAsh : public component_updater::ComponentManagerAsh {
 public:
  MockComponentManagerAsh() = default;

  MockComponentManagerAsh(const MockComponentManagerAsh&) = delete;
  MockComponentManagerAsh& operator=(const MockComponentManagerAsh&) = delete;

  MOCK_METHOD(void, SetDelegate, (Delegate * delegate), (override));
  MOCK_METHOD(void,
              Load,
              (const std::string& name,
               MountPolicy mount_policy,
               UpdatePolicy update_policy,
               LoadCallback load_callback),
              (override));
  MOCK_METHOD(bool, Unload, (const std::string& name), (override));
  MOCK_METHOD(void,
              GetVersion,
              (const std::string& name,
               base::OnceCallback<void(const base::Version&)> version_callback),
              (const, override));
  MOCK_METHOD(void,
              RegisterCompatiblePath,
              (const std::string& name,
               component_updater::CompatibleComponentInfo info),
              (override));
  MOCK_METHOD(void,
              UnregisterCompatiblePath,
              (const std::string& name),
              (override));
  MOCK_METHOD(base::FilePath,
              GetCompatiblePath,
              (const std::string& name),
              (const, override));
  MOCK_METHOD(bool,
              IsRegisteredMayBlock,
              (const std::string& name),
              (override));
  MOCK_METHOD(void, RegisterInstalled, (), (override));

 protected:
  ~MockComponentManagerAsh() override = default;
};

}  // namespace

namespace crosapi {

FakeBrowserManager::FakeBrowserManager()
    : BrowserManager(base::MakeRefCounted<MockComponentManagerAsh>()) {}

FakeBrowserManager::~FakeBrowserManager() = default;

void FakeBrowserManager::SetGetFeedbackDataResponse(
    base::Value::Dict response) {
  feedback_response_ = std::move(response);
}

void FakeBrowserManager::SignalMojoDisconnected() {
  OnMojoDisconnected();
}

void FakeBrowserManager::StartRunning() {
  SetState(State::RUNNING);
}

void FakeBrowserManager::StopRunning() {
  SetState(State::STOPPED);
}

void FakeBrowserManager::NewFullscreenWindow(
    const GURL& url,
    BrowserManager::NewFullscreenWindowCallback callback) {
  std::move(callback).Run(new_fullscreen_window_creation_result_);
}

void FakeBrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  // Run |callback| with the pre-set |feedback_responses_|, unless testing
  // client requests waiting for crosapi mojo disconnected event being observed.
  if (!wait_for_mojo_disconnect_) {
    std::move(callback).Run(std::move(feedback_response_));
  }
}

void FakeBrowserManager::InitializeAndStartIfNeeded() {
  StartRunning();
}

void FakeBrowserManager::OnSessionStateChanged() {}

}  // namespace crosapi
