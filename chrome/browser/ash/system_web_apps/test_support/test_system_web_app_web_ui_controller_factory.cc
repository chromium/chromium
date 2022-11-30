// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_web_ui_controller_factory.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_url_data_source.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

// WebUIController that serves a System Web App.
class TestSystemWebAppWebUIController : public content::WebUIController {
 public:
  explicit TestSystemWebAppWebUIController(const std::string& source_name,
                                           content::WebUI* web_ui)
      : WebUIController(web_ui) {
    AddTestURLDataSource(source_name,
                         web_ui->GetWebContents()->GetBrowserContext());
  }
  TestSystemWebAppWebUIController(const TestSystemWebAppWebUIController&) =
      delete;
  TestSystemWebAppWebUIController& operator=(
      const TestSystemWebAppWebUIController&) = delete;
};

}  // namespace

TestSystemWebAppWebUIControllerFactory::TestSystemWebAppWebUIControllerFactory(
    std::string source_name)
    : source_name_(std::move(source_name)) {}

std::unique_ptr<content::WebUIController>
TestSystemWebAppWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme) ||
      url.host_piece() != source_name_) {
    return nullptr;
  }

  return std::make_unique<TestSystemWebAppWebUIController>(source_name_,
                                                           web_ui);
}

content::WebUI::TypeID TestSystemWebAppWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) {
  if (UseWebUIForURL(browser_context, url))
    return reinterpret_cast<content::WebUI::TypeID>(1);

  return content::WebUI::kNoWebUI;
}

bool TestSystemWebAppWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == source_name_;
}

}  // namespace ash
