// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_H_
#define CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/persistent_renderer_prefs.mojom.h"

class PrefService;

namespace content {
class RenderFrameHost;
}

// This class is used for updating profile-specific renderer preferences
// from within blink. See
// third_party/blink/public/mojom/persistent_renderer_prefs.mojom for more
// details.
class PersistentRendererPrefsManager
    : public KeyedService,
      public blink::mojom::PersistentRendererPrefsService {
 public:
  explicit PersistentRendererPrefsManager(PrefService& pref_service);

  PersistentRendererPrefsManager(const PersistentRendererPrefsManager&) =
      delete;
  PersistentRendererPrefsManager& operator=(
      const PersistentRendererPrefsManager&) = delete;

  ~PersistentRendererPrefsManager() override;

  static void BindFrameReceiver(
      content::RenderFrameHost* frame,
      mojo::PendingReceiver<blink::mojom::PersistentRendererPrefsService>
          receiver);

 private:
  friend class TestPersistentRendererPrefsManager;

  void SetViewSourceLineWrapping(bool value) override;

  const raw_ref<PrefService> pref_service_;
  mojo::ReceiverSet<blink::mojom::PersistentRendererPrefsService> receivers_;
};

#endif  // CHROME_BROWSER_PREFS_PERSISTENT_RENDERER_PREFS_MANAGER_H_
