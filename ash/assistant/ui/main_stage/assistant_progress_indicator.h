// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_

#include "base/component_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantProgressIndicator
    : public views::View {
  METADATA_HEADER(AssistantProgressIndicator, views::View)

 public:
  AssistantProgressIndicator();

  AssistantProgressIndicator(const AssistantProgressIndicator&) = delete;
  AssistantProgressIndicator& operator=(const AssistantProgressIndicator&) =
      delete;

  ~AssistantProgressIndicator() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnLayerOpacityChanged(ui::PropertyChangeReason reason) override;
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;

 private:
  void InitLayout();

  // Caches the last call to VisibilityChanged. Because we trigger this event
  // artificially, we want to make sure that we don't over trigger.
  bool is_drawn_ = false;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
