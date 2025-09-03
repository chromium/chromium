// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_

#include <memory>

#include "chrome/browser/glic/host/host.h"

namespace glic {

class GlicView;

class GlicUiEmbedder : public Host::Delegate {
 public:
  // Show the glic UI.
  virtual void Show() = 0;

  // Create the WebView in which to show glic.
  virtual std::unique_ptr<GlicView> CreateGlicView() = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_UI_EMBEDDER_H_
