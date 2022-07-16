// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

class AmbientAssistantContainerView;
class AmbientViewDelegate;
class PhotoView;

// Container view to display all Ambient Mode related views, i.e. photo frame,
// weather info.
class ASH_EXPORT AmbientContainerView : public views::View {
 public:
  METADATA_HEADER(AmbientContainerView);

  explicit AmbientContainerView(AmbientViewDelegate* delegate);
  ~AmbientContainerView() override;

 private:
  friend class AmbientAshTestBase;

  void Init();

  AmbientViewDelegate* delegate_ = nullptr;

  // Owned by view hierarchy.
  PhotoView* photo_view_ = nullptr;
  AmbientAssistantContainerView* ambient_assistant_container_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_CONTAINER_VIEW_H_
