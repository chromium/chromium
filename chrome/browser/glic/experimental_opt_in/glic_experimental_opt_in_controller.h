// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view_tracker.h"

class Profile;

namespace content {
class WebContents;
}

namespace views {
class Widget;
}

namespace glic {

// Controller for the experimental triggering opt-in flow.
class GlicExperimentalOptInController {
 public:
  explicit GlicExperimentalOptInController(Profile* profile);
  GlicExperimentalOptInController(const GlicExperimentalOptInController&) =
      delete;
  GlicExperimentalOptInController& operator=(
      const GlicExperimentalOptInController&) = delete;
  ~GlicExperimentalOptInController();

  views::Widget* ShowDialog(content::WebContents* web_contents);
  void CloseDialog();

 private:
  raw_ptr<Profile> profile_;
  views::ViewTracker view_tracker_;

  base::WeakPtrFactory<GlicExperimentalOptInController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_EXPERIMENTAL_OPT_IN_GLIC_EXPERIMENTAL_OPT_IN_CONTROLLER_H_
