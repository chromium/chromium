// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_NORMAL_CAPTURE_BAR_VIEW_H_
#define ASH_CAPTURE_MODE_NORMAL_CAPTURE_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class CaptureModeBehavior;
class CaptureModeSourceView;
class CaptureModeTypeView;

// A view that acts as the content view of the capture mode bar widget for a
// normal capture session. It has a set of buttons to toggle between image and
// video capture, and another set of buttons to toggle between fullscreen,
// region, and window capture sources. It also contains a settings button. The
// structure looks like this:
//
//   +---------------------------------------------------------------+
//   |  +----------------+  |                       |                |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  |  |   |  |   |  |  |  |   |  |   |  |   |  |  |   |  |   |  |
//   |  |  +---+  +---+  |  |  +---+  +---+  +---+  |  +---+  +---+  |
//   |  +----------------+  |  ^                 ^  |  ^      ^      |
//   +--^----------------------|-----------------|-----|------|------+
//   ^  |                      +-----------------+     |      |
//   |  |                      |                       |      IconButton
//   |  |                      |                       |
//   |  |                      |                       IconButton
//   |  |                      CaptureModeSourceView
//   |  CaptureModeTypeView
//   |
//   NormalCaptureBarView
//
class ASH_EXPORT NormalCaptureBarView : public CaptureModeBarView {
  METADATA_HEADER(NormalCaptureBarView, CaptureModeBarView)

 public:
  // The `active_behavior` decides the capture bar configurations.
  explicit NormalCaptureBarView(CaptureModeBehavior* active_behavior);
  NormalCaptureBarView(const NormalCaptureBarView&) = delete;
  NormalCaptureBarView& operator=(const NormalCaptureBarView&) = delete;
  ~NormalCaptureBarView() override;

  // CaptureModeBarView:
  CaptureModeTypeView* GetCaptureTypeView() const override;
  CaptureModeSourceView* GetCaptureSourceView() const override;
  void OnCaptureSourceChanged(CaptureModeSource new_source) override;
  void OnCaptureTypeChanged(CaptureModeType new_type) override;

 private:
  raw_ptr<CaptureModeTypeView> capture_type_view_ = nullptr;
  raw_ptr<CaptureModeSourceView> capture_source_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_NORMAL_CAPTURE_BAR_VIEW_H_
