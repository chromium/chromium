// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"

namespace commerce {

namespace {

void SetUpWebUIDataSource(content::WebUI* web_ui,
                          const char* web_ui_host,
                          base::span<const webui::ResourcePath> resources,
                          int default_resource) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), web_ui_host);
  webui::SetupWebUIDataSource(source, resources, default_resource);
  // Disable CSP for tests so that EvalJS can be invoked without CSP violations.
  source->DisableContentSecurityPolicy();
}

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  explicit TestWebUIControllerFactory(MockShoppingService* shopping_service)
      : shopping_service_(shopping_service) {}

  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.host_piece() == kChromeUICommerceInternalsHost) {
      return std::make_unique<CommerceInternalsUI>(
          web_ui,
          base::BindOnce(&SetUpWebUIDataSource, web_ui,
                         kChromeUICommerceInternalsHost),
          shopping_service_.get());
    }

    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.host_piece() == kChromeUICommerceInternalsHost) {
      return kChromeUICommerceInternalsHost;
    }

    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }

 private:
  raw_ptr<MockShoppingService> shopping_service_;
};

}  // namespace

class CommerceInternalsBrowserTest : public MojoWebUIBrowserTest {
 public:
  CommerceInternalsBrowserTest() {
    shopping_service_ = std::make_unique<MockShoppingService>();
    factory_ =
        std::make_unique<TestWebUIControllerFactory>(shopping_service_.get());
    content::WebUIControllerFactory::RegisterFactory(factory_.get());
  }
  ~CommerceInternalsBrowserTest() override = default;

  void NavigateToInternalsPage() {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        GURL(content::GetWebUIURLString(kChromeUICommerceInternalsHost))));
  }

 protected:
  std::unique_ptr<MockShoppingService> shopping_service_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;
};

// Test that the internals page opens.
IN_PROC_BROWSER_TEST_F(CommerceInternalsBrowserTest, InternalsPageOpen) {
  NavigateToInternalsPage();
  content::WebContents* internals_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(internals_web_contents->GetWebUI() != nullptr);

  // Verify the title of the page.
  EXPECT_EQ(
      "commerce internals",
      EvalJs(internals_web_contents,
             "(function() {return document.title.toLocaleLowerCase();}) ()"));
}

}  // namespace commerce
