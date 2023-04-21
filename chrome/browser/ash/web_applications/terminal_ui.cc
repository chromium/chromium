// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/terminal_ui.h"

#include "chrome/browser/ash/web_applications/terminal_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

TerminalUIConfig::TerminalUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedTerminalHost) {}

TerminalUIConfig::~TerminalUIConfig() = default;

std::unique_ptr<content::WebUIController>
TerminalUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                        const GURL& url) {
  return std::make_unique<TerminalUI>(web_ui);
}

TerminalUI::TerminalUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, TerminalSource::ForTerminal(profile));
}

TerminalUI::~TerminalUI() = default;
