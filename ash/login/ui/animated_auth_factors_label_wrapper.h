// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_ANIMATED_AUTH_FACTORS_LABEL_WRAPPER_H_
#define ASH_LOGIN_UI_ANIMATED_AUTH_FACTORS_LABEL_WRAPPER_H_

#include "ash/ash_export.h"
#include "ash/login/ui/auth_factor_model.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// TODO(b/216696664): Add unit tests for this class.
class ASH_EXPORT AnimatedAuthFactorsLabelWrapper : public views::View {
  METADATA_HEADER(AnimatedAuthFactorsLabelWrapper, views::View)

 public:
  AnimatedAuthFactorsLabelWrapper();
  AnimatedAuthFactorsLabelWrapper(const AnimatedAuthFactorsLabelWrapper&) =
      delete;
  AnimatedAuthFactorsLabelWrapper& operator=(
      const AnimatedAuthFactorsLabelWrapper&) = delete;
  ~AnimatedAuthFactorsLabelWrapper() override;

  void SetLabelTextAndAccessibleName(int label_id,
                                     int accessible_name_id,
                                     bool animate = false);

  views::Label* label() { return current_label_; }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  int previous_label_id_ = 0;
  int previous_accessible_name_id_ = 0;

  // |current_label_| is the main label that's visible when animations are not
  // active. It should at all times have the current text. |previous_label_| is
  // a non-accessible label that replaces |current_label_| at the start of the
  // animation so that |previous_label_| can fade out while |current_label_|
  // fades in.
  raw_ptr<views::Label> previous_label_;
  raw_ptr<views::Label> current_label_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_ANIMATED_AUTH_FACTORS_LABEL_WRAPPER_H_
