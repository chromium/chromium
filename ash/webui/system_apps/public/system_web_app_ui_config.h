// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_UI_CONFIG_H_
#define ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_UI_CONFIG_H_

#include <memory>
#include <string>
#include <string_view>

#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

namespace internal {

// Internal base class that overrides IsWebUIEnabled. Separate from the
// templated class because IsWebUIEnabled needs to be implemented in //chrome.
class BaseSystemWebAppUIConfig : public content::WebUIConfig {
 public:
  BaseSystemWebAppUIConfig(SystemWebAppType swa_type,
                           std::string_view scheme,
                           std::string_view host)
      : content::WebUIConfig(scheme, host), swa_type_(swa_type) {}

  // Implemented in //chrome/browser/ash/system_web_apps/
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

 private:
  SystemWebAppType swa_type_;
};

}  // namespace internal

// Default WebUIConfig for the chrome:// component of System Web Apps. It
// has an implementation of `CreateWebUIController()`, which returns a new
// `T` and an implementation of `IsWebUIEnabled()` which returns true if
// System Web Apps are enabled and `swa_type` is enabled. Optionally,
// it can take a CreateWebUIControllerFunc that callers can use to inject
// dependencies into T.
template <typename T>
class SystemWebAppUIConfig : public internal::BaseSystemWebAppUIConfig {
 public:
  using CreateWebUIControllerFunc = base::RepeatingCallback<std::unique_ptr<
      content::WebUIController>(content::WebUI*, const GURL& url)>;

  // Constructs a WebUIConfig for chrome://`host` and enables it if
  // System Web Apps are enabled and `swa_type` is enabled.
  SystemWebAppUIConfig(std::string_view host, SystemWebAppType swa_type)
      : SystemWebAppUIConfig(
            host,
            swa_type,
            base::BindRepeating(
                [](content::WebUI* web_ui, const GURL& url)
                    -> std::unique_ptr<content::WebUIController> {
                  return std::make_unique<T>(web_ui);
                })) {}

  // Same as above, but takes in an extra `create_controller_func` argument that
  // can be used to pass a function to construct T. Used when we need to inject
  // dependencies into T e.g. T needs a delegate that is implemented in
  // //chrome.
  SystemWebAppUIConfig(std::string_view host,
                       SystemWebAppType swa_type,
                       CreateWebUIControllerFunc create_controller_func)
      : BaseSystemWebAppUIConfig(swa_type, content::kChromeUIScheme, host),
        create_controller_func_(create_controller_func) {
    static_assert(!std::is_base_of_v<ui::UntrustedWebUIController, T>,
                  "Should only be used for chrome:// WebUIs. See "
                  "SystemWebAppUntrustedUIConfig below for chrome-untrusted:// "
                  "WebUIs.");
  }

  ~SystemWebAppUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return create_controller_func_.Run(web_ui, url);
  }

 private:
  CreateWebUIControllerFunc create_controller_func_;
};

// Default WebUIConfig for the chrome-untrusted:// component of System Web Apps.
// It has an implementation of `CreateWebUIController()`, which returns a new
// `T` and an implementation of `IsWebUIEnabled()` which returns true if
// System Web Apps are enabled and `swa_type` is enabled.
template <typename T>
class SystemWebAppUntrustedUIConfig
    : public internal::BaseSystemWebAppUIConfig {
 public:
  // Constructs a WebUIConfig for chrome://`host` and enables it if
  // System Web Apps are enabled and `swa_type` is enabled.
  SystemWebAppUntrustedUIConfig(std::string_view host,
                                SystemWebAppType swa_type)
      : BaseSystemWebAppUIConfig(swa_type,
                                 content::kChromeUIUntrustedScheme,
                                 host) {
    static_assert(std::is_base_of<ui::UntrustedWebUIController, T>::value,
                  "Should only be used for chrome-untrusted:// WebUIs. See "
                  "SystemWebAppUIConfig above for chrome:// WebUIs.");
  }

  ~SystemWebAppUntrustedUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<T>(web_ui);
  }
};

}  //  namespace ash

#endif  // ASH_WEBUI_SYSTEM_APPS_PUBLIC_SYSTEM_WEB_APP_UI_CONFIG_H_
