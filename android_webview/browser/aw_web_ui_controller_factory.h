// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace android_webview {

class AwWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  static AwWebUIControllerFactory* GetInstance();

  // content::WebUIControllerFactory overrides
  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override;
  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override;
  bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                              const GURL& url) override;
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  friend struct base::DefaultSingletonTraits<AwWebUIControllerFactory>;

  AwWebUIControllerFactory();
  ~AwWebUIControllerFactory() override;

  DISALLOW_COPY_AND_ASSIGN(AwWebUIControllerFactory);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_UI_CONTROLLER_FACTORY_H_
