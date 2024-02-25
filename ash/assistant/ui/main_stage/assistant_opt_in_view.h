// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_

#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Button;
class StyledLabel;
}  // namespace views

namespace ash {

class AssistantViewDelegate;

// AssistantOptInView ----------------------------------------------------------

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOptInView
    : public views::View,
      public AssistantStateObserver {
  METADATA_HEADER(AssistantOptInView, views::View)

 public:
  explicit AssistantOptInView(AssistantViewDelegate* delegate_);
  AssistantOptInView(const AssistantOptInView&) = delete;
  AssistantOptInView& operator=(const AssistantOptInView&) = delete;
  ~AssistantOptInView() override;

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override;

 private:
  void InitLayout();
  void UpdateLabel(int consent_status);

  void OnButtonPressed();

  raw_ptr<views::StyledLabel> label_;  // Owned by view hierarchy.

  raw_ptr<views::Button> container_;

  raw_ptr<AssistantViewDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_OPT_IN_VIEW_H_
