// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_tools_desktop.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
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

BrowserWindowInterface* ResolveActiveBrowserForProfile(
    base::WeakPtr<Profile> weak_profile) {
  Profile* profile = weak_profile.get();
  if (!profile) {
    return nullptr;
  }
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile);
  if (!collection) {
    return nullptr;
  }
  // Iterate `ForEach` directly rather than calling `FindTabbedBrowser()` so a
  // detached/floating Glic window on another workspace still resolves the
  // profile's most recently active normal browser window across workspaces.
  BrowserWindowInterface* active_browser = nullptr;
  collection->ForEach(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
            !browser->IsDeleteScheduled()) {
          active_browser = browser;
          return false;
        }
        return true;
      },
      BrowserCollection::Order::kActivation);
  return active_browser;
}

}  // namespace

std::unique_ptr<GlicToolsHolder> CreateAiOverlayToolsForGlic(
    Profile* profile,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver) {
  if (!profile || !GetGlicProviderSpec(profile).SupportsFeature(
                      GlicFeature::kDynamicChromeTools)) {
    return nullptr;
  }
  return std::make_unique<GlicToolsHolderImpl>(ttc::AiOverlayTools::Create(
      std::move(receiver),
      base::BindRepeating(&ResolveActiveBrowserForProfile,
                          profile->GetWeakPtr()),
      /*page_context_monitor=*/nullptr));
}

}  // namespace glic
