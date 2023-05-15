// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_CONFIG_H_

#include "ash/webui/projector_app/untrusted_projector_annotator_ui.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUIDataSource;
class WebUIController;
class WebUI;
}  // namespace content

// Implementation of the chromeos::UntrustedProjectorAnnotatorUIDelegate to
// expose some //chrome functions to //chromeos.
class ChromeUntrustedProjectorAnnotatorUIDelegate
    : public ash::UntrustedProjectorAnnotatorUIDelegate {
 public:
  ChromeUntrustedProjectorAnnotatorUIDelegate();
  ChromeUntrustedProjectorAnnotatorUIDelegate(
      const ChromeUntrustedProjectorAnnotatorUIDelegate&) = delete;
  ChromeUntrustedProjectorAnnotatorUIDelegate& operator=(
      const ChromeUntrustedProjectorAnnotatorUIDelegate&) = delete;

  // ash::UntrustedProjectorAnnotatorUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
};

// A webui config for the chrome-untrusted:// part of Projector annotator.
class UntrustedProjectorAnnotatorUIConfig : public content::WebUIConfig {
 public:
  UntrustedProjectorAnnotatorUIConfig();
  UntrustedProjectorAnnotatorUIConfig(
      const UntrustedProjectorAnnotatorUIConfig& other) = delete;
  UntrustedProjectorAnnotatorUIConfig& operator=(
      const UntrustedProjectorAnnotatorUIConfig&) = delete;
  ~UntrustedProjectorAnnotatorUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_APP_UNTRUSTED_PROJECTOR_ANNOTATOR_UI_CONFIG_H_
