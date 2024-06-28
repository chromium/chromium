// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_H_
#define ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// Defines a base view that will be used as the content view of the annotations
// overlay widget, which is added as a child window of the surface on which
// annotations are triggered.
// It's defined here since Ash cannot depend directly on `content/` and this
// view can host a |views::WebView| and its associated |WebContents|, to show
// ink annotations.
class ASH_PUBLIC_EXPORT AnnotationsOverlayView : public views::View {
  METADATA_HEADER(AnnotationsOverlayView, views::View)

 public:
  ~AnnotationsOverlayView() override;

 protected:
  AnnotationsOverlayView();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ANNOTATOR_ANNOTATIONS_OVERLAY_VIEW_H_
