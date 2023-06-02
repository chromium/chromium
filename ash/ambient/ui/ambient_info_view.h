// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_

#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/glanceable_info_view.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Transform;
}

namespace views {
class Label;
}  // namespace views

namespace ash {

class GlanceableInfoView;

class ASH_EXPORT AmbientInfoView : public views::View,
                                   public GlanceableInfoView::Delegate {
 public:
  METADATA_HEADER(AmbientInfoView);

  explicit AmbientInfoView(AmbientViewDelegate* delegate);
  AmbientInfoView(const AmbientInfoView&) = delete;
  AmbientInfoView& operator=(AmbientInfoView&) = delete;
  ~AmbientInfoView() override;

  // views::View:
  void OnThemeChanged() override;

  // GlanceableInfoView::Delegate:
  SkColor GetTimeTemperatureFontColor() override;

  void UpdateImageDetails(const std::u16string& details,
                          const std::u16string& related_details);

  void SetTextTransform(const gfx::Transform& transform);

  int GetAdjustedLeftPaddingToMatchBottom();

 private:
  void InitLayout();

  // Owned by |AmbientController| and should always outlive |this|.
  raw_ptr<AmbientViewDelegate, ExperimentalAsh> delegate_ = nullptr;

  raw_ptr<GlanceableInfoView, ExperimentalAsh> glanceable_info_view_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> details_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> related_details_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_INFO_VIEW_H_
