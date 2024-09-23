// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_PROGRESS_BAR_DIALOG_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_PROGRESS_BAR_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace arc {

class ProgressBarDialogView : public views::BoxLayoutView {
 public:
  explicit ProgressBarDialogView(bool is_multiple_files);
  ProgressBarDialogView(const ProgressBarDialogView&) = delete;
  ProgressBarDialogView& operator=(const ProgressBarDialogView&) = delete;
  ~ProgressBarDialogView() override;

  static void Show(aura::Window* parent, ProgressBarDialogView* view);

  void UpdateProgressBarValue(double value);
  double GetProgressBarValue() const;

  // Interpolate next progress bar value based on previous value, step size,
  // step factor so that progress bar value will keep progressing despite
  // missing actual update from source. Interpolated process will slow down
  // logarithmically based on step factor as value approaches 1.0.
  void UpdateInterpolatedProgressBarValue();

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void OnThemeChanged() override;

 private:
  // Progress bar view to show file streaming progress to the user.
  raw_ptr<views::ProgressBar, DanglingUntriaged> progress_bar_ = nullptr;

  // Message label for the progress bar.
  raw_ptr<views::Label, DanglingUntriaged> message_label_ = nullptr;

  // Indicates whether multiple files are being shared for UI string.
  const bool is_multiple_files_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_UI_PROGRESS_BAR_DIALOG_VIEW_H_
