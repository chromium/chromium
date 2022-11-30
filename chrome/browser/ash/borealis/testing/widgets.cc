// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/widgets.h"

#include "components/exo/shell_surface_util.h"

namespace borealis {

std::unique_ptr<views::Widget> CreateFakeWidget(std::string name,
                                                bool fullscreen /*=false*/) {
  ash::TestWidgetBuilder builder;
  builder.SetShow(false);
  std::unique_ptr<views::Widget> widget = builder.BuildOwnsNativeWidget();
  exo::SetShellApplicationId(widget->GetNativeWindow(), name);
  if (fullscreen) {
    widget->SetFullscreen(true);
  }
  widget->Show();
  return widget;
}

}  // namespace borealis
