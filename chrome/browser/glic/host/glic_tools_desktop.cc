// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_tools_desktop.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

namespace glic {
namespace {

class GlicToolsHolderImpl : public GlicToolsHolder {
 public:
  explicit GlicToolsHolderImpl(std::unique_ptr<ttc::AiOverlayTools> tools)
      : tools_(std::move(tools)) {}
  ~GlicToolsHolderImpl() override = default;

 private:
  std::unique_ptr<ttc::AiOverlayTools> tools_;
};

}  // namespace

std::unique_ptr<GlicToolsHolder> CreateAiOverlayToolsForGlic(
    Profile* profile,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver) {
  if (!base::FeatureList::IsEnabled(features::kGlicDynamicChromeTools)) {
    return nullptr;
  }
  BrowserWindowInterface* active_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            browser->GetProfile() == profile) {
          active_browser = browser;
          return false;
        }
        return true;
      });
  return std::make_unique<GlicToolsHolderImpl>(
      std::make_unique<ttc::AiOverlayTools>(std::move(receiver), active_browser,
                                            /*page_context_monitor=*/nullptr));
}

}  // namespace glic
