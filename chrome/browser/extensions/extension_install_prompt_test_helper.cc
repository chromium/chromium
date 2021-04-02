// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

ExtensionInstallPromptTestHelper::ExtensionInstallPromptTestHelper() {}
ExtensionInstallPromptTestHelper::ExtensionInstallPromptTestHelper(
    base::OnceClosure quit_closure)
    : quit_closure_(std::move(quit_closure)) {}
ExtensionInstallPromptTestHelper::~ExtensionInstallPromptTestHelper() {}

ExtensionInstallPrompt::DoneCallback
ExtensionInstallPromptTestHelper::GetCallback() {
  return base::BindOnce(&ExtensionInstallPromptTestHelper::HandleResult,
                        base::Unretained(this));
}

ExtensionInstallPrompt::Result
ExtensionInstallPromptTestHelper::result() const {
  if (!result_.get()) {
    ADD_FAILURE() << "Result was never set!";
    return ExtensionInstallPrompt::Result::ACCEPTED;  // Avoid crashing.
  }
  return *result_;
}

void ExtensionInstallPromptTestHelper::ClearResultForTesting() {
  if (!result_.get()) {
    ADD_FAILURE() << "Result was never set!";
    return;
  }
  result_.reset();
}

void ExtensionInstallPromptTestHelper::HandleResult(
    ExtensionInstallPrompt::Result result) {
  if (result_.get())
    ADD_FAILURE() << "HandleResult() called twice!";
  if (quit_closure_)
    std::move(quit_closure_).Run();
  result_ = std::make_unique<ExtensionInstallPrompt::Result>(result);
}
