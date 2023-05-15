// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_QUERY_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_QUERY_VIEW_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/view.h"

namespace ash {

class AssistantQuery;

// AssistantQueryView is the visual representation of an AssistantQuery. It is a
// child view of UiElementContainerView.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantQueryView : public views::View {
 public:
  AssistantQueryView();

  AssistantQueryView(const AssistantQueryView&) = delete;
  AssistantQueryView& operator=(const AssistantQueryView&) = delete;

  ~AssistantQueryView() override;

  // views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnThemeChanged() override;

  void SetQuery(const AssistantQuery& query);

 private:
  void InitLayout();
  void SetText(const std::string& high_confidence_text,
               const std::string& low_confidence_text = std::string());

  raw_ptr<views::Label, ExperimentalAsh>
      high_confidence_label_;  // Owned by view hierarchy.
  raw_ptr<views::Label, ExperimentalAsh>
      low_confidence_label_;  // Owned by view hierarchy.
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_QUERY_VIEW_H_
