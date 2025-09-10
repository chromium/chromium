// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_

#include <memory>

#include "chrome/browser/glic/host/host.h"
#include "ui/views/view.h"

namespace views {
class View;
}

namespace glic {

class GlicUiEmbedder {
 public:
  virtual ~GlicUiEmbedder() = default;

  // Returns the Host::Delegate if this embedder uses one.
  virtual Host::Delegate* GetHostDelegate() = 0;

  // Show the glic UI.
  virtual void Show() = 0;

  // Create the WebView in which to show glic.
  virtual std::unique_ptr<views::View> CreateView() = 0;

  // Creates the inactive version of this embedder.
  virtual std::unique_ptr<GlicUiEmbedder> CreateInactiveEmbedder() const = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_
