// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_TOOLS_DESKTOP_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_TOOLS_DESKTOP_H_

#include <memory>

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

class Profile;

namespace glic {

class GlicToolsHolder {
 public:
  virtual ~GlicToolsHolder() = default;
};

// Creates and returns the desktop tools controller for Glic, or nullptr if
// disabled.
std::unique_ptr<GlicToolsHolder> CreateAiOverlayToolsForGlic(
    Profile* profile,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_TOOLS_DESKTOP_H_
