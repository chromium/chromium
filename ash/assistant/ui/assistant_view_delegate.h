// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_

#include <map>
#include <string>

#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

class AssistantNotificationModel;
enum class AssistantButtonId;

namespace assistant {
namespace util {
enum class DeepLinkType;
}  // namespace util
}  // namespace assistant

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegateObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(AssistantButtonId id) {}

  // Invoked when the dialog plate contents have been committed.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) {}

  // Invoked when Assistant onboarding is shown.
  virtual void OnOnboardingShown() {}

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() {}

  // Invoked when a suggestion UI element is pressed.
  virtual void OnSuggestionPressed(
      const base::UnguessableToken& suggestion_id) {}

  // Invoked when a launcher search chip is pressed.
  virtual void OnLauncherSearchChipPressed(const std::u16string& query) {}
};

// A delegate of views in assistant/ui that handles views related actions e.g.
// get models for the views, adding observers, closing the views, opening urls,
// etc.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegate {
 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  virtual ~AssistantViewDelegate() = default;

  // Gets the notification model.
  virtual const AssistantNotificationModel* GetNotificationModel() const = 0;

  // Adds/removes the specified view delegate observer.
  virtual void AddObserver(AssistantViewDelegateObserver* observer) = 0;
  virtual void RemoveObserver(AssistantViewDelegateObserver* observer) = 0;

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  virtual void DownloadImage(const GURL& url,
                             ImageDownloader::DownloadCallback callback) = 0;

  // Returns the cursor_manager.
  virtual ::wm::CursorManager* GetCursorManager() = 0;

  // Returns the given name of the primary user.
  virtual std::string GetPrimaryUserGivenName() const = 0;

  // Returns the root window for the specified |display_id|.
  virtual aura::Window* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the root window that newly created windows should be added to.
  virtual aura::Window* GetRootWindowForNewWindows() = 0;

  // Returns true if in tablet mode.
  virtual bool IsTabletMode() const = 0;

  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(AssistantButtonId id) = 0;

  // Invoked when the dialog plate contents have been committed.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) = 0;

  // Invoked when an in-Assistant notification button is pressed.
  virtual void OnNotificationButtonPressed(const std::string& notification_id,
                                           int notification_button_index) = 0;

  // Invoked when Assistant onboarding is shown.
  virtual void OnOnboardingShown() = 0;

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() = 0;

  // Invoked when suggestion UI is pressed.
  virtual void OnSuggestionPressed(
      const base::UnguessableToken& suggestion_id) = 0;

  // Returns true if Assistant onboarding should be shown.
  virtual bool ShouldShowOnboarding() const = 0;

  // Invoked when a launcher search chip is pressed.
  virtual void OnLauncherSearchChipPressed(const std::u16string& query) = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
