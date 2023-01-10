// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_
#define ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash::curtain {

// The root view shown as the security curtain overlay when the security curtain
// is created by an enterprise admin through the 'start crd session' remote
// command.
class ASH_EXPORT RemoteMaintenanceCurtainView : public views::FlexLayoutView {
 public:
  RemoteMaintenanceCurtainView();
  RemoteMaintenanceCurtainView(const RemoteMaintenanceCurtainView&) = delete;
  RemoteMaintenanceCurtainView& operator=(const RemoteMaintenanceCurtainView&) =
      delete;
  ~RemoteMaintenanceCurtainView() override;

  METADATA_HEADER(RemoteMaintenanceCurtainView);

 private:
  void Initialize();
};

}  // namespace ash::curtain

#endif  // ASH_CURTAIN_REMOTE_MAINTENANCE_CURTAIN_VIEW_H_
