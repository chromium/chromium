// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A view that displays a row of action buttons near the capture region.
class ASH_EXPORT ActionButtonContainerView : public views::View {
  METADATA_HEADER(ActionButtonContainerView, views::View)

 public:
  ActionButtonContainerView();
  ActionButtonContainerView(const ActionButtonContainerView&) = delete;
  ActionButtonContainerView& operator=(const ActionButtonContainerView&) =
      delete;
  ~ActionButtonContainerView() override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
