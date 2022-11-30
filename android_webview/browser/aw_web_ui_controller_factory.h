// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace android_webview {

class AwWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  static AwWebUIControllerFactory* GetInstance();

  AwWebUIControllerFactory(const AwWebUIControllerFactory&) = delete;
  AwWebUIControllerFactory& operator=(const AwWebUIControllerFactory&) = delete;

  // content::WebUIControllerFactory overrides
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  friend struct base::DefaultSingletonTraits<AwWebUIControllerFactory>;

  AwWebUIControllerFactory();
  ~AwWebUIControllerFactory() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_
