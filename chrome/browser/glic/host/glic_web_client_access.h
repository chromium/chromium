// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_

// Interface to the glic web client, provided by the glic WebUI.
#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// Access to the glic web client, from outside of the WebUI handler.
class GlicWebClientAccess {
 public:
  using PanelWillOpenCallback = mojom::WebClient::NotifyPanelWillOpenCallback;

  // Informs the web client that the panel will open. The panel should not be
  // shown until `done` is called.
  virtual void PanelWillOpen(mojom::PanelOpeningDataPtr panel_opening_data,
                             PanelWillOpenCallback done) = 0;

  // Informs the web client the panel was closed (no longer visible). The web
  // client should not be destroyed until after `done` is called.
  virtual void PanelWasClosed(base::OnceClosure done) = 0;

  // Informs the web client when the user starts and finishes dragging to resize
  // the panel.
  virtual void ManualResizeChanged(bool resizing) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_ACCESS_H_
