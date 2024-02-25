// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_
#define ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ash {

// Default WebUIConfig for ChromeOS WebUI. It has an implementation of
// `CreateWebUIController()`, which returns a new `T`. Optionally, it can take a
// CreateWebUIControllerFunc that callers can use to inject dependencies into T.
template <typename T>
class ChromeOSWebUIConfig : public content::WebUIConfig {
 public:
  using CreateWebUIControllerFunc = base::RepeatingCallback<std::unique_ptr<
      content::WebUIController>(content::WebUI*, const GURL& url)>;

  static std::unique_ptr<content::WebUIController>
  DefaultCreateWebUIControllerFunc(content::WebUI* web_ui, const GURL& url) {
    // We need to determine the correct WebUIController
    // constructor to use at compile time, depending on whether it
    // requires only WebUI* or both WebUI* and GURL. We currently
    // don't support WebUIControllers that have two constructors
    // where one has a single WebUI* arg and the other has both
    // WebUI* and GURL params.
    static_assert(!(std::is_constructible_v<T, content::WebUI*> &&
                    std::is_constructible_v<T, content::WebUI*, GURL>));
    if constexpr (std::is_constructible_v<T, content::WebUI*, GURL>) {
      return std::make_unique<T>(web_ui, url);
    } else {
      return std::make_unique<T>(web_ui);
    }
  }

  // Constructs a WebUIConfig for a ChromeOS WebUI.
  ChromeOSWebUIConfig(std::string_view scheme, std::string_view host)
      : ChromeOSWebUIConfig(
            scheme,
            host,
            base::BindRepeating(&DefaultCreateWebUIControllerFunc)) {}

  // Same as above, but takes in an extra `create_controller_func` argument that
  // can be used to pass a function to construct T. Used when we need to inject
  // dependencies into T e.g. T needs a delegate that is implemented in
  // //chrome.
  ChromeOSWebUIConfig(std::string_view scheme,
                      std::string_view host,
                      CreateWebUIControllerFunc create_controller_func)
      : WebUIConfig(scheme, host),
        create_controller_func_(create_controller_func) {}

  ~ChromeOSWebUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return create_controller_func_.Run(web_ui, url);
  }

 private:
  CreateWebUIControllerFunc create_controller_func_;
};

}  //  namespace ash

#endif  //  ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_
