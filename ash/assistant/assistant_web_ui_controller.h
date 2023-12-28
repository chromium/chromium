// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/assistant/assistant_web_view_delegate_impl.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

class AssistantWebContainerView;
class AssistantWebContainerEventObserver;

// The class to manage Assistant web container view.
class ASH_EXPORT AssistantWebUiController : public views::WidgetObserver,
                                            public AssistantControllerObserver,
                                            public AssistantStateObserver {
 public:
  AssistantWebUiController();

  AssistantWebUiController(const AssistantWebUiController&) = delete;
  AssistantWebUiController& operator=(const AssistantWebUiController&) = delete;

  ~AssistantWebUiController() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) override;

  // AssistantStateObserver:
  void OnAssistantSettingsEnabled(bool enabled) override;

  void OnBackButtonPressed();

  AssistantWebContainerView* GetViewForTest();

 private:
  void ShowUi(const GURL& url);
  void CloseUi();

  // Constructs/resets |web_container_view_|.
  void CreateWebContainerView();
  void ResetWebContainerView();

  AssistantWebViewDelegateImpl view_delegate_;

  // Owned by view hierarchy.
  raw_ptr<AssistantWebContainerView> web_container_view_ = nullptr;

  // Observes key press events on the |web_container_view_|.
  std::unique_ptr<AssistantWebContainerEventObserver> event_observer_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_WEB_UI_CONTROLLER_H_
