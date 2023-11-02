// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/mixin_based_extension_apitest.h"

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

MixinBasedExtensionApiTest::MixinBasedExtensionApiTest() = default;

MixinBasedExtensionApiTest::~MixinBasedExtensionApiTest() = default;

void MixinBasedExtensionApiTest::SetUp() {
  mixin_host_.SetUp();
  ExtensionApiTest::SetUp();
}

void MixinBasedExtensionApiTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpCommandLine(command_line);
  ExtensionApiTest::SetUpCommandLine(command_line);
}

void MixinBasedExtensionApiTest::SetUpDefaultCommandLine(
    base::CommandLine* command_line) {
  mixin_host_.SetUpDefaultCommandLine(command_line);
  ExtensionApiTest::SetUpDefaultCommandLine(command_line);
}

bool MixinBasedExtensionApiTest::SetUpUserDataDirectory() {
  return mixin_host_.SetUpUserDataDirectory() &&
         ExtensionApiTest::SetUpUserDataDirectory();
}

void MixinBasedExtensionApiTest::SetUpInProcessBrowserTestFixture() {
  mixin_host_.SetUpInProcessBrowserTestFixture();
  ExtensionApiTest::SetUpInProcessBrowserTestFixture();
}

void MixinBasedExtensionApiTest::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  mixin_host_.CreatedBrowserMainParts(browser_main_parts);
  ExtensionApiTest::CreatedBrowserMainParts(browser_main_parts);
}

void MixinBasedExtensionApiTest::SetUpOnMainThread() {
  mixin_host_.SetUpOnMainThread();
  ExtensionApiTest::SetUpOnMainThread();
}

void MixinBasedExtensionApiTest::TearDownOnMainThread() {
  mixin_host_.TearDownOnMainThread();
  ExtensionApiTest::TearDownOnMainThread();
}

void MixinBasedExtensionApiTest::TearDownInProcessBrowserTestFixture() {
  mixin_host_.TearDownInProcessBrowserTestFixture();
  ExtensionApiTest::TearDownInProcessBrowserTestFixture();
}

void MixinBasedExtensionApiTest::TearDown() {
  mixin_host_.TearDown();
  ExtensionApiTest::TearDown();
}

}  // namespace extensions
