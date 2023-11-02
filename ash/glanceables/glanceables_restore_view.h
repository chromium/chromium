// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
#define ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class ImageButton;
}  // namespace views

namespace ash {

class PillButton;

// Glanceables screen button that triggers session restores. Shows a screenshot
// of the previous session, or a text button if there is no screenshot.
class ASH_EXPORT GlanceablesRestoreView : public views::View {
 public:
  GlanceablesRestoreView();
  GlanceablesRestoreView(const GlanceablesRestoreView&) = delete;
  GlanceablesRestoreView& operator=(const GlanceablesRestoreView&) = delete;
  ~GlanceablesRestoreView() override;

 private:
  friend class GlanceablesTest;

  void OnSignoutScreenshotDecoded(const gfx::ImageSkia& image);

  // Adds an image button with a screenshot image.
  void AddImageButton(const gfx::ImageSkia& image);

  // Adds a "Restore" pill button.
  void AddPillButton();

  views::ImageButton* image_button_ = nullptr;
  PillButton* pill_button_ = nullptr;
  base::WeakPtrFactory<GlanceablesRestoreView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_RESTORE_VIEW_H_
