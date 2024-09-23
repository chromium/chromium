// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"

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
  AccessibilityLabelsBubbleModel(const AccessibilityLabelsBubbleModel&) =
      delete;
  AccessibilityLabelsBubbleModel& operator=(
      const AccessibilityLabelsBubbleModel&) = delete;

  // ConfirmBubbleModel implementation.
  std::u16string GetTitle() const override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(ui::mojom::DialogButton button) const override;
  void Accept() override;
  void Cancel() override;
  std::u16string GetLinkText() const override;
  GURL GetHelpPageURL() const override;
  void OpenHelpPage() override;

 private:
  // Set the profile preferences to enable or disable the feature.
  void SetPref(bool enabled);

  // Unowned.
  raw_ptr<Profile> profile_;

  base::WeakPtr<content::WebContents> web_contents_;

  // Whether to always enable or just enable once.
  bool enable_always_;
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_ACCESSIBILITY_LABELS_BUBBLE_MODEL_H_
