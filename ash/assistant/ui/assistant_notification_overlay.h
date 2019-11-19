// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_
#define ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_

#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_overlay.h"
#include "base/component_export.h"
#include "base/macros.h"

namespace ash {

class AssistantViewDelegate;

// AssistantNotificationOverlay is a pseudo-child of AssistantMainView which is
// responsible for parenting in-Assistant notifications.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantNotificationOverlay
    : public AssistantOverlay,
      public AssistantUiModelObserver,
      public AssistantNotificationModelObserver {
 public:
  AssistantNotificationOverlay(AssistantViewDelegate* delegate);
  ~AssistantNotificationOverlay() override;

  // AssistantOverlay:
  const char* GetClassName() const override;
  LayoutParams GetLayoutParams() const override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

  // AssistantUiModelObserver:
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // AssistantNotificationModelObserver:
  void OnNotificationAdded(const AssistantNotification* notification) override;

 private:
  void InitLayout();

  AssistantViewDelegate* const delegate_;  // Owned by AssistantController.

  DISALLOW_COPY_AND_ASSIGN(AssistantNotificationOverlay);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_NOTIFICATION_OVERLAY_H_
