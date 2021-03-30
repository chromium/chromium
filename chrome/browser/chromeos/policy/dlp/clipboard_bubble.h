// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_CLIPBOARD_BUBBLE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_CLIPBOARD_BUBBLE_H_

#include <string>

#include "base/callback.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ImageView;
class LabelButton;
}  // namespace views

namespace policy {

// This inline bubble shown for restricted copy/paste.
class ClipboardBubbleView : public views::View {
 public:
  METADATA_HEADER(ClipboardBubbleView);

  explicit ClipboardBubbleView(const std::u16string& text);
  ~ClipboardBubbleView() override;

  virtual gfx::Size GetBubbleSize() const = 0;

 protected:
  // This function should get called if the view got updated e.g. AddChildView.
  void UpdateBorderSize(const gfx::Size& size);

  views::Label* label_ = nullptr;
  views::ImageView* managed_icon_ = nullptr;
  views::ImageView* border_ = nullptr;
};

class ClipboardBlockBubble : public ClipboardBubbleView {
 public:
  METADATA_HEADER(ClipboardBlockBubble);

  explicit ClipboardBlockBubble(const std::u16string& text);
  ~ClipboardBlockBubble() override;

  // ClipboardBubbleView::
  gfx::Size GetBubbleSize() const override;

  void SetDismissCallback(base::RepeatingCallback<void()> cb);

 private:
  views::LabelButton* button_ = nullptr;
};

class ClipboardWarnBubble : public ClipboardBubbleView {
 public:
  METADATA_HEADER(ClipboardWarnBubble);

  explicit ClipboardWarnBubble(const std::u16string& text);
  ~ClipboardWarnBubble() override;

  // ClipboardBubbleView::
  gfx::Size GetBubbleSize() const override;

  void SetDismissCallback(base::RepeatingCallback<void()> cb);

  void SetProceedCallback(base::RepeatingCallback<void()> cb);

 private:
  views::LabelButton* cancel_button_ = nullptr;
  views::LabelButton* paste_button_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_CLIPBOARD_BUBBLE_H_
