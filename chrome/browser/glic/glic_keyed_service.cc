// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_keyed_service.h"

#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
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
  if (!window_controller_) {
    window_controller_ = std::make_unique<GlicWindowController>(
        Profile::FromBrowserContext(browser_context_));
  }
  window_controller_->Show();
}
