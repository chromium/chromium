// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace glic {
class GlicView;

// Class for Glic window controller. Owned by the Glic profile keyed-service.
// This gets created when the Glic window needs to be shown and it owns the Glic
// widget.
class GlicWindowController {
 public:
  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  explicit GlicWindowController(Profile* profile);
  ~GlicWindowController();

  // Shows the glic window.
  void Show();

  // Sets the size of the glic window to the specified dimensions. Returns true
  // if the operation succeeded.
  bool Resize(const gfx::Size& size);

  // Returns the current size of the glic window.
  gfx::Size GetSize();

  // Called to notify the controller that the window was requested to be closed.
  void Close();

 private:
  const raw_ptr<Profile> profile_;
  views::UniqueWidgetPtr widget_;
  // Owned by widget_.
  raw_ptr<glic::GlicView> glic_view_ = nullptr;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
