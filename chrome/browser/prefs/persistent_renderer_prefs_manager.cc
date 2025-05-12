// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/persistent_renderer_prefs_manager.h"

#include "chrome/browser/prefs/persistent_renderer_prefs_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"

PersistentRendererPrefsManager::PersistentRendererPrefsManager(
    PrefService& pref_service)
    : pref_service_(pref_service) {}

PersistentRendererPrefsManager::~PersistentRendererPrefsManager() = default;

void PersistentRendererPrefsManager::SetViewSourceLineWrapping(bool value) {
  pref_service_->SetBoolean(prefs::kViewSourceLineWrappingEnabled, value);
}

void PersistentRendererPrefsManager::BindFrameReceiver(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::PersistentRendererPrefsService>
        receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());

  auto* persistent_renderer_prefs_manager =
      PersistentRendererPrefsManagerFactory::GetInstance()->GetForProfile(
          profile);
  if (!persistent_renderer_prefs_manager) {
    return;
  }

  persistent_renderer_prefs_manager->receivers_.Add(
      persistent_renderer_prefs_manager, std::move(receiver));
}
