// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/test/personalization_app_browsertest_fixture.h"

#include <memory>

#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_ui_delegate.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"

std::unique_ptr<content::WebUIController>
TestPersonalizationAppWebUIProvider::NewWebUI(content::WebUI* web_ui,
                                              const GURL& url) {
  auto delegate = std::make_unique<FakePersonalizationAppUiDelegate>(web_ui);
  return std::make_unique<ash::PersonalizationAppUI>(web_ui,
                                                     std::move(delegate));
}

void PersonalizationAppBrowserTestFixture::SetUpOnMainThread() {
  MojoWebUIBrowserTest::SetUpOnMainThread();
  test_factory_.AddFactoryOverride(ash::kChromeUIPersonalizationAppHost,
                                   &test_web_ui_provider_);
}
