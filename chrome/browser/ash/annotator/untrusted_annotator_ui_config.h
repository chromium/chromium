// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_CONFIG_H_
#define CHROME_BROWSER_ASH_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_CONFIG_H_

#include "ash/webui/annotator/untrusted_annotator_ui.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUIDataSource;
class WebUIController;
class WebUI;
}  // namespace content

// Implementation of the chromeos::UntrustedAnnotatorUIDelegate to
// expose some //chrome functions to //chromeos.
class ChromeUntrustedAnnotatorUIDelegate
    : public ash::UntrustedAnnotatorUIDelegate {
 public:
  ChromeUntrustedAnnotatorUIDelegate();
  ChromeUntrustedAnnotatorUIDelegate(
      const ChromeUntrustedAnnotatorUIDelegate&) = delete;
  ChromeUntrustedAnnotatorUIDelegate& operator=(
      const ChromeUntrustedAnnotatorUIDelegate&) = delete;

  // ash::UntrustedAnnotatorUIDelegate:
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
};

// A webui config for the chrome-untrusted:// part of Projector annotator.
class UntrustedAnnotatorUIConfig : public content::WebUIConfig {
 public:
  UntrustedAnnotatorUIConfig();
  UntrustedAnnotatorUIConfig(const UntrustedAnnotatorUIConfig& other) = delete;
  UntrustedAnnotatorUIConfig& operator=(const UntrustedAnnotatorUIConfig&) =
      delete;
  ~UntrustedAnnotatorUIConfig() override;

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
};

#endif  // CHROME_BROWSER_ASH_ANNOTATOR_UNTRUSTED_ANNOTATOR_UI_CONFIG_H_
