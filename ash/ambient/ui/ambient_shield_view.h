// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_SHIELD_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_SHIELD_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AmbientShieldView : public views::View {
  METADATA_HEADER(AmbientShieldView, views::View)

 public:
  AmbientShieldView();
  AmbientShieldView(const AmbientShieldView&) = delete;
  AmbientShieldView& operator=(const AmbientShieldView&) = delete;
  ~AmbientShieldView() override;

 private:
  void InitLayout();
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_SHIELD_VIEW_H_
