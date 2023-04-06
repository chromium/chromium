// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_
#define ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace ash {

// Default WebUIConfig for ChromeOS WebUI. It has an implementation of
// `CreateWebUIController()`, which returns a new `T`. Optionally, it can take a
// CreateWebUIControllerFunc that callers can use to inject dependencies into T.
template <typename T>
class ChromeOSWebUIConfig : public content::WebUIConfig {
 public:
  using CreateWebUIControllerFunc =
      std::unique_ptr<content::WebUIController> (*)(content::WebUI*);

  // Constructs a WebUIConfig for a ChromeOS WebUI.
  ChromeOSWebUIConfig(base::StringPiece scheme, base::StringPiece host)
      : ChromeOSWebUIConfig(scheme,
                            host,
                            [](content::WebUI* web_ui)
                                -> std::unique_ptr<content::WebUIController> {
                              return std::make_unique<T>(web_ui);
                            }) {}

  // Same as above, but takes in an extra `create_controller_func` argument that
  // can be used to pass a function to construct T. Used when we need to inject
  // dependencies into T e.g. T needs a delegate that is implemented in
  // //chrome.
  ChromeOSWebUIConfig(base::StringPiece scheme,
                      base::StringPiece host,
                      CreateWebUIControllerFunc create_controller_func)
      : WebUIConfig(scheme, host),
        create_controller_func_(create_controller_func) {}

  ~ChromeOSWebUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override {
    return create_controller_func_(web_ui);
  }

 private:
  CreateWebUIControllerFunc create_controller_func_;
};

}  //  namespace ash

#endif  //  ASH_WEBUI_COMMON_CHROME_OS_WEBUI_CONFIG_H_
