// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantProgressIndicator
    : public views::View {
 public:
  AssistantProgressIndicator();
  ~AssistantProgressIndicator() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnLayerOpacityChanged(ui::PropertyChangeReason reason) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

 private:
  void InitLayout();

  // Caches the last call to VisibilityChanged. Because we trigger this event
  // artificially, we want to make sure that we don't over trigger.
  bool is_drawn_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssistantProgressIndicator);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
