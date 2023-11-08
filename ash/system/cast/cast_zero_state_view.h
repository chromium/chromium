// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAST_CAST_ZERO_STATE_VIEW_H_
#define ASH_SYSTEM_CAST_CAST_ZERO_STATE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// The view shown in the system tray when there are no cast targets available.
class CastZeroStateView : public views::View {
  METADATA_HEADER(CastZeroStateView, views::View)

 public:
  CastZeroStateView();
  CastZeroStateView(const CastZeroStateView&) = delete;
  CastZeroStateView& operator=(const CastZeroStateView&) = delete;
  ~CastZeroStateView() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAST_CAST_ZERO_STATE_VIEW_H_
