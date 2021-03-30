// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/fake_browser_manager.h"

#include "chrome/browser/component_updater/cros_component_manager.h"

namespace crosapi {

FakeBrowserManager::FakeBrowserManager()
    : BrowserManager(
          scoped_refptr<component_updater::CrOSComponentManager>(nullptr)) {}

FakeBrowserManager::~FakeBrowserManager() = default;

void FakeBrowserManager::SetGetFeedbackDataResponse(base::Value response) {
  feedback_response_ = std::move(response);
}

void FakeBrowserManager::SignalMojoDisconnected() {
  SetState(State::TERMINATING);
}

bool FakeBrowserManager::IsRunning() const {
  return is_running_;
}

bool FakeBrowserManager::IsRunningOrWillRun() const {
  return is_running_;
}

void FakeBrowserManager::GetFeedbackData(GetFeedbackDataCallback callback) {
  const base::DictionaryValue* sysinfo_entries;
  feedback_response_.GetAsDictionary(&sysinfo_entries);

  // Run |callback| with the pre-set |feedback_responses_|, unless testing
  // client requests waiting for crosapi mojo disconnected event being observed.
  if (!wait_for_mojo_disconnect_)
    std::move(callback).Run(std::move(feedback_response_));
}

}  // namespace crosapi
