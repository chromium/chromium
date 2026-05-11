// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/dictation_menu_observer.h"

#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"

namespace dictation {

DictationMenuObserver::DictationMenuObserver(RenderViewContextMenuProxy* proxy,
                                             BrowserWindowInterface* bwi)
    : window_(bwi), proxy_(proxy) {
  CHECK(proxy_);
}

DictationMenuObserver::~DictationMenuObserver() = default;

void DictationMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DictationKeyedService* service = GetDictationService();
  if (service && service->ShouldShowContextMenuItem()) {
    CHECK(base::FeatureList::IsEnabled(kDictation));
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_DICTATION,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_DICTATION));
  }
}

bool DictationMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_DICTATION;
}

bool DictationMenuObserver::IsCommandIdEnabled(int command_id) {
  CHECK_EQ(command_id, IDC_CONTENT_CONTEXT_DICTATION);
  return true;
}

void DictationMenuObserver::ExecuteCommand(int command_id) {
  CHECK_EQ(command_id, IDC_CONTENT_CONTEXT_DICTATION);
  if (!proxy_->GetRenderFrameHost()) {
    return;
  }

  DictationKeyedService* service = GetDictationService();
  if (service) {
    service->ContextMenuHandler(*window_);
  }
}

DictationKeyedService* DictationMenuObserver::GetDictationService() {
  content::BrowserContext* context = proxy_->GetBrowserContext();
  if (!context) {
    return nullptr;
  }

  return DictationKeyedService::Get(context);
}

}  // namespace dictation
