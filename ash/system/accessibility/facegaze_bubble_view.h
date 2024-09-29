// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// The FaceGaze bubble view. This is a UI that appears at the top of the screen,
// which tells the user the most recently recognized facial gesture and the
// corresponding action that was taken. This view is only visible when the
// FaceGaze feature is enabled.
class ASH_EXPORT FaceGazeBubbleView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(FaceGazeBubbleView, views::BubbleDialogDelegateView)

 public:
  FaceGazeBubbleView();
  FaceGazeBubbleView(const FaceGazeBubbleView&) = delete;
  FaceGazeBubbleView& operator=(const FaceGazeBubbleView&) = delete;
  ~FaceGazeBubbleView() override;

  // Updates text content of this view.
  void Update(const std::u16string& text);

  // views::BubbleDialogDelegateView:
  void OnThemeChanged() override;

  const std::u16string& GetTextForTesting() const;

 private:
  friend class FaceGazeBubbleControllerTest;

  // An image that displays the FaceGaze logo.
  raw_ptr<views::ImageView> image_ = nullptr;

  // A label that displays the most recently recognized gesture and
  // corresponding action.
  raw_ptr<views::Label> label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FACEGAZE_BUBBLE_VIEW_H_
