// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_gemini_web_warming_observer.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicGeminiWebWarmingObserver::GlicGeminiWebWarmingObserver(
    tabs::TabInterface* tab)
    : content::WebContentsObserver(tab->GetContents()),
      tab_subscription_(tab->RegisterWillDiscardContents(base::BindRepeating(
          &GlicGeminiWebWarmingObserver::WillDiscardContents,
          base::Unretained(this)))) {}

GlicGeminiWebWarmingObserver::~GlicGeminiWebWarmingObserver() = default;

void GlicGeminiWebWarmingObserver::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  Observe(new_contents);
}

// Observe `PrimaryPageChanged` rather than `DidFinishNavigation` so this only
// runs when the tab's primary main document actually changes (including
// prerender/BFCache activations), automatically ignoring subframe navigations,
// same-document SPA navigations, uncommitted navigations, and inactive
// prerenders.
void GlicGeminiWebWarmingObserver::PrimaryPageChanged(content::Page& page) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

  if (!base::FeatureList::IsEnabled(features::kGlicWarmOnGeminiWeb)) {
    return;
  }

  if (!web_contents() || IsGlicGuest(web_contents())) {
    return;
  }

  if (!IsGeminiOrigin(page.GetMainDocument().GetLastCommittedOrigin())) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  if (!GlicEnabling::IsEnabledForProfile(profile)) {
    return;
  }

  if (GlicKeyedService* glic_service = GlicKeyedService::Get(profile)) {
    // If a warmed container already exists, `TryPreload()` is a no-op.
    glic_service->TryPreload(GlicWarmingTrigger::kGeminiWeb);
    Observe(nullptr);
    tab_subscription_ = {};
  }
}

}  // namespace glic
