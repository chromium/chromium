// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/views/widget/unique_widget_ptr.h"

// Class for Glic window controller. Owned by the Glic profile keyed-service.
// This gets created when the Glic window needs to be shown and it owns the Glic
// widget.
class GlicWindowController {
 public:
  static GlicWindowController* GetOrCreateGlicWindowController(
      Profile* profile);

  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  explicit GlicWindowController(Profile* profile);
  ~GlicWindowController();

  // Shows the glic window.
  void Show();

  // Called to notify the controller that the window was requested to be closed.
  void Close();

  // // Returns a WeakPtr to this instance. It can be destroyed at any time if
  // the profile is deleted or if the browser shuts down.
  base::WeakPtr<GlicWindowController> GetWeakPtr();

 private:
  raw_ptr<Profile> profile_;
  views::UniqueWidgetPtr widget_;

  base::WeakPtrFactory<GlicWindowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
