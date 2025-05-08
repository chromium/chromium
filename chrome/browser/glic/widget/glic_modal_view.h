// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_VIEW_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"

namespace views {
class Label;
class ImageButton;
}  // namespace views

namespace glic {
// Creates a notification view with rounded corners, displaying a message and a
// close button. Intended to be used when it is needed to show modals over the
// glic widget.
class GlicModalView : public views::BoxLayoutView {
 public:
  explicit GlicModalView(
      const ui::ColorProvider* color_provider,
      const std::u16string& label_text,
      base::RepeatingClosure close_callback = base::RepeatingClosure());

  GlicModalView(const GlicModalView&) = delete;
  GlicModalView& operator=(const GlicModalView&) = delete;

  ~GlicModalView() override;

 private:
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  base::RepeatingClosure close_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_MODAL_VIEW_H_
