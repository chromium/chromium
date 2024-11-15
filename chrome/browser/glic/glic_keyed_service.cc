// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

GlicKeyedService::GlicKeyedService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

GlicKeyedService::~GlicKeyedService() = default;

void GlicKeyedService::LaunchUI() {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser) {
    GURL web_ui_url(std::string("chrome://glic"));
    NavigateParams params(browser, web_ui_url,
                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    Navigate(&params);
  } else {
    LOG(ERROR) << "No active browser found to launch Glic UI.";
  }
}
