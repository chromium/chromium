// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXO_PARTS_H_
#define CHROME_BROWSER_EXO_PARTS_H_

#include <memory>

namespace ash {
class ArcOverlayManager;
}

namespace exo {
class WaylandServerController;
}

class ExoParts {
 public:
  // Creates ExoParts. Returns null if exo should not be created.
  static std::unique_ptr<ExoParts> CreateIfNecessary();

  ExoParts(const ExoParts&) = delete;
  ExoParts& operator=(const ExoParts&) = delete;

  ~ExoParts();

 private:
  ExoParts();

  std::unique_ptr<ash::ArcOverlayManager> arc_overlay_manager_;
  std::unique_ptr<exo::WaylandServerController> wayland_server_;
};

#endif  // CHROME_BROWSER_EXO_PARTS_H_
