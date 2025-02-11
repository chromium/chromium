// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_

#include "base/memory/weak_ptr.h"

class Browser;
class Profile;
namespace views {
class Widget;
}

namespace content {
class WebContents;
}

namespace glic {

class GlicFreDialogView;

// This class owns and manages the glic FRE dialog, and is owned by a
// GlicWindowController.
class GlicFreController {
 public:
  GlicFreController(const GlicFreController&) = delete;
  GlicFreController& operator=(const GlicFreController&) = delete;

  GlicFreController();
  ~GlicFreController();

  // Returns whether the FRE dialog should be shown.
  bool ShouldShowFreDialog(Profile* profile);

  // Returns whether the FRE can be shown.
  bool CanShowFreDialog(Browser* browser);

  void ShowFreDialog(Profile* profile, Browser* browser);

  // Closes the FRE modal dialog and immediately opens a glic window attached to
  // the same browser.
  void AcceptFre(Profile* profile);
  // Closes the FRE modal dialog.
  void DismissFre();

  // Returns the WebContents from the dialog view.
  content::WebContents* GetWebContents();

 private:
  std::unique_ptr<views::Widget> fre_widget_;
  raw_ptr<GlicFreDialogView> fre_view_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_FRE_CONTROLLER_H_
