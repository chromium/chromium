// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
#define ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_

#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class AssistantControllerImpl;

class AssistantViewDelegateImpl : public AssistantViewDelegate {
 public:
  explicit AssistantViewDelegateImpl(
      AssistantControllerImpl* assistant_controller);

  AssistantViewDelegateImpl(const AssistantViewDelegateImpl&) = delete;
  AssistantViewDelegateImpl& operator=(const AssistantViewDelegateImpl&) =
      delete;

  ~AssistantViewDelegateImpl() override;

  // AssistantViewDelegate:
  const AssistantNotificationModel* GetNotificationModel() const override;
  void AddObserver(AssistantViewDelegateObserver* observer) override;
  void RemoveObserver(AssistantViewDelegateObserver* observer) override;
  void DownloadImage(const GURL& url,
                     ImageDownloader::DownloadCallback callback) override;
  ::wm::CursorManager* GetCursorManager() override;
  std::string GetPrimaryUserGivenName() const override;
  aura::Window* GetRootWindowForDisplayId(int64_t display_id) override;
  aura::Window* GetRootWindowForNewWindows() override;
  bool IsTabletMode() const override;
  void OnDialogPlateButtonPressed(AssistantButtonId id) override;
  void OnDialogPlateContentsCommitted(const std::string& text) override;
  void OnNotificationButtonPressed(const std::string& notification_id,
                                   int notification_button_index) override;
  void OnOnboardingShown() override;
  void OnOptInButtonPressed() override;
  void OnSuggestionPressed(
      const base::UnguessableToken& suggestion_id) override;
  bool ShouldShowOnboarding() const override;
  void OnLauncherSearchChipPressed(const std::u16string& query) override;

 private:
  const raw_ptr<AssistantControllerImpl> assistant_controller_;
  base::ObserverList<AssistantViewDelegateObserver> view_delegate_observers_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_VIEW_DELEGATE_IMPL_H_
