// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// A view that acts as the contents view of the camera preview widget. It will
// be responsible for painting the latest camera video frame inside its bounds.
class CameraPreviewView : public views::View {
 public:
  METADATA_HEADER(CameraPreviewView);

  CameraPreviewView();
  CameraPreviewView(const CameraPreviewView&) = delete;
  CameraPreviewView& operator=(const CameraPreviewView&) = delete;
  ~CameraPreviewView() override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
