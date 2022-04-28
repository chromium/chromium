// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/testing/widgets.h"

#include "components/exo/shell_surface_util.h"
#include "ui/aura/client/aura_constants.h"

namespace borealis {

std::unique_ptr<views::Widget> CreateFakeWidget(std::string name,
                                                bool fullscreen /*=false*/) {
  std::unique_ptr<views::Widget> widget =
      ash::TestWidgetBuilder().SetShow(false).BuildOwnsNativeWidget();
  exo::SetShellApplicationId(widget->GetNativeWindow(), name);
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize);
  if (fullscreen) {
    widget->SetFullscreen(true);
  }
  widget->Show();
  return widget;
}

}  // namespace borealis
