// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/dictation_menu_observer.h"

#include "base/feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"

namespace dictation {

DictationMenuObserver::DictationMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(*proxy) {}

DictationMenuObserver::~DictationMenuObserver() = default;

void DictationMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DictationKeyedService* service = GetDictationService();
  if (service && service->ShouldShowContextMenuItem()) {
    CHECK(base::FeatureList::IsEnabled(kDictation));
    proxy_->AddMenuItem(
        IDC_CONTENT_CONTEXT_DICTATION,
        l10n_util::GetStringUTF16(IDS_DICTATION_CONTEXT_MENU_STRING));
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
  content::RenderFrameHost* rfh = proxy_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  DictationKeyedService* service = GetDictationService();
  if (service) {
    service->ContextMenuHandler(*rfh);
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
