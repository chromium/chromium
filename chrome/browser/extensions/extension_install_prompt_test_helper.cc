// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

ExtensionInstallPromptTestHelper::ExtensionInstallPromptTestHelper() {}
ExtensionInstallPromptTestHelper::ExtensionInstallPromptTestHelper(
    base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)) {}
ExtensionInstallPromptTestHelper::~ExtensionInstallPromptTestHelper() {}

ExtensionInstallPrompt::DoneCallback
ExtensionInstallPromptTestHelper::GetCallback() {
  return base::BindOnce(&ExtensionInstallPromptTestHelper::HandlePayload,
                        base::Unretained(this));
}

ExtensionInstallPrompt::DoneCallbackPayload
ExtensionInstallPromptTestHelper::payload() const {
  if (!payload_.get()) {
    ADD_FAILURE() << "Payload was never set!";
    return ExtensionInstallPrompt::DoneCallbackPayload(
        ExtensionInstallPrompt::Result::ACCEPTED);  // Avoid crashing.
  }
  return *payload_;
}

ExtensionInstallPrompt::Result ExtensionInstallPromptTestHelper::result()
    const {
  if (!payload_.get()) {
    ADD_FAILURE() << "Payload was never set!";
    return ExtensionInstallPrompt::Result::ACCEPTED;  // Avoid crashing.
  }
  return payload_->result;
}

std::string ExtensionInstallPromptTestHelper::justification() const {
  if (!payload_.get()) {
    ADD_FAILURE() << "Payload was never set!";
    return std::string();  // Avoid crashing.
  }
  return payload_->justification;
}

void ExtensionInstallPromptTestHelper::ClearPayloadForTesting() {
  if (!payload_.get()) {
    ADD_FAILURE() << "Payload was never set!";
    return;
  }
  payload_.reset();
}

void ExtensionInstallPromptTestHelper::HandlePayload(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  if (payload_.get())
    ADD_FAILURE() << "HandlePayload() called twice!";
  if (quit_closure_)
    std::move(quit_closure_).Run();
  payload_ = std::make_unique<ExtensionInstallPrompt::DoneCallbackPayload>(
      std::move(payload));
}
