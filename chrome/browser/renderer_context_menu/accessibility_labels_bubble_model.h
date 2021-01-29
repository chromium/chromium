// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_

#include "base/macros.h"
#include "chrome/browser/ui/confirm_bubble_model.h"

class Profile;

namespace content {
class WebContents;
}

// A class that implements a bubble menu shown when we confirm a user allows
// integrating the accessibility labels service of Google to Chrome.
class AccessibilityLabelsBubbleModel : public ConfirmBubbleModel {
 public:
  AccessibilityLabelsBubbleModel(Profile* profile,
                                 content::WebContents* web_contents,
                                 bool enable_always);
  ~AccessibilityLabelsBubbleModel() override;

  // ConfirmBubbleModel implementation.
  base::string16 GetTitle() const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(ui::DialogButton button) const override;
  void Accept() override;
  void Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetHelpPageURL() const override;
  void OpenHelpPage() override;

 private:
  // Set the profile preferences to enable or disable the feature.
  void SetPref(bool enabled);

  Profile* profile_;
  content::WebContents* web_contents_;
  bool enable_always_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityLabelsBubbleModel);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_
