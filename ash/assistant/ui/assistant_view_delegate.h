// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_

#include <map>
#include <string>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_notification_model.h"
#include "ash/assistant/model/assistant_notification_model_observer.h"
#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/model/assistant_ui_model_observer.h"
#include "ash/assistant/ui/assistant_mini_view.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/public/cpp/assistant/assistant_image_downloader.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace assistant {
namespace util {
enum class DeepLinkType;
}  // namespace util
}  // namespace assistant

class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegateObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  // Invoked when Assistant has received a deep link of the specified |type|
  // with the given |params|.
  virtual void OnDeepLinkReceived(
      assistant::util::DeepLinkType type,
      const std::map<std::string, std::string>& params) {}

  // Invoked when the dialog plate button identified by |id| is pressed.
  virtual void OnDialogPlateButtonPressed(AssistantButtonId id) {}

  // Invoked when the dialog plate contents have been committed.
  virtual void OnDialogPlateContentsCommitted(const std::string& text) {}

  // Invoked when the mini view is pressed.
  virtual void OnMiniViewPressed() {}

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() {}

  // Invoked when the proactive suggestions close button is pressed.
  virtual void OnProactiveSuggestionsCloseButtonPressed() {}

  // Invoked when the hover state of the proactive suggestions view is changed.
  virtual void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {}

  // Invoked when the proactive suggestions view is pressed.
  virtual void OnProactiveSuggestionsViewPressed() {}

  // Invoked when a suggestion chip is pressed.
  virtual void OnSuggestionChipPressed(const AssistantSuggestion* suggestion) {}
};

// A delegate of views in assistant/ui that handles views related actions e.g.
// get models for the views, adding observers, closing the views, opening urls,
// etc.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantViewDelegate {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;

  virtual ~AssistantViewDelegate() {}

  // Gets the interaction model associated with the view delegate.
  virtual const AssistantInteractionModel* GetInteractionModel() const = 0;

  // Gets the notification model associated with the view delegate.
  virtual const AssistantNotificationModel* GetNotificationModel() const = 0;

  // Gets the suggestions model associated with the view delegate.
  virtual const AssistantSuggestionsModel* GetSuggestionsModel() const = 0;

  // Gets the ui model associated with the view delegate.
  virtual const AssistantUiModel* GetUiModel() const = 0;

  // Adds/removes the specified view delegate observer.
  virtual void AddObserver(AssistantViewDelegateObserver* observer) = 0;
  virtual void RemoveObserver(AssistantViewDelegateObserver* observer) = 0;

  // Adds/removes the interaction model observer associated with the view
  // delegate.
  virtual void AddInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;
  virtual void RemoveInteractionModelObserver(
      AssistantInteractionModelObserver* observer) = 0;

  // Adds/removes the notification model observer associated with the view
  // delegate.
  virtual void AddNotificationModelObserver(
      AssistantNotificationModelObserver* observer) = 0;
  virtual void RemoveNotificationModelObserver(
      AssistantNotificationModelObserver* observer) = 0;

  // Adds/removes the suggestions model observer associated with the view
  // delegate.
  virtual void AddSuggestionsModelObserver(
      AssistantSuggestionsModelObserver* observer) = 0;
  virtual void RemoveSuggestionsModelObserver(
      AssistantSuggestionsModelObserver* observer) = 0;

  // Adds/removes the ui model observer associated with the view delegate.
  virtual void AddUiModelObserver(AssistantUiModelObserver* observer) = 0;
  virtual void RemoveUiModelObserver(AssistantUiModelObserver* observer) = 0;

  // Gets the caption bar delegate associated with the view delegate.
  virtual CaptionBarDelegate* GetCaptionBarDelegate() = 0;

  // Downloads the image found at the specified |url|. On completion, the
  // supplied |callback| will be run with the downloaded image. If the download
  // attempt is unsuccessful, a NULL image is returned.
  virtual void DownloadImage(
      const GURL& url,
      AssistantImageDownloader::DownloadCallback callback) = 0;

  // Returns the cursor_manager.
  virtual ::wm::CursorManager* GetCursorManager() = 0;

  // Acquires a NavigableContentsFactory from the Content Service to allow
  // Assistant to display embedded web contents.
  virtual void GetNavigableContentsFactoryForView(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory>
          receiver) = 0;

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

  // Invoked when the mini view is pressed.
  virtual void OnMiniViewPressed() = 0;

  // Invoked when an in-Assistant notification button is pressed.
  virtual void OnNotificationButtonPressed(const std::string& notification_id,
                                           int notification_button_index) = 0;

  // Invoked when the opt in button is pressed.
  virtual void OnOptInButtonPressed() {}

  // Invoked when the proactive suggestions close button is pressed.
  virtual void OnProactiveSuggestionsCloseButtonPressed() {}

  // Invoked when the hover state of the proactive suggestions view is changed.
  virtual void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) {}

  // Invoked when the proactive suggestions view is pressed.
  virtual void OnProactiveSuggestionsViewPressed() {}

  // Invoked when suggestion chip is pressed.
  virtual void OnSuggestionChipPressed(
      const AssistantSuggestion* suggestion) = 0;

  // Opens the specified |url| in a new browser tab. Special handling is applied
  // to deep links which may cause deviation from this behavior.
  virtual void OpenUrlFromView(const GURL& url) = 0;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_VIEW_DELEGATE_H_
