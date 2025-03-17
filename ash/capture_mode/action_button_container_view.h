// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
#define ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Label;
class Link;
}

namespace ash {

class ActionButtonView;
class SystemShadow;

// A view that displays a row of action buttons near the capture region. It may
// display an error message if actions are not available.
class ASH_EXPORT ActionButtonContainerView : public views::View {
  METADATA_HEADER(ActionButtonContainerView, views::View)

 public:
  // A view that displays an error message and icon. It may optionally also show
  // a try again link.
  class ASH_EXPORT ErrorView : public views::BoxLayoutView {
    METADATA_HEADER(ErrorView, views::BoxLayoutView)

   public:
    ErrorView();
    ErrorView(const ErrorView&) = delete;
    ErrorView& operator=(const ErrorView&) = delete;
    ~ErrorView() override;

    views::Link* try_again_link() { return try_again_link_; }

    // views::BoxLayoutView:
    void SetVisible(bool visible) override;
    void AddedToWidget() override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

    // Sets the error message to show on the error view.
    void SetErrorMessage(const std::u16string& error_message);

    // Sets the callback to run when the try again link is pressed. Note that
    // the try again link is only shown if `try_again_callback` is not null.
    void SetTryAgainCallback(base::RepeatingClosure try_again_callback);

    std::u16string_view GetErrorMessageForTesting() const;

   private:
    std::unique_ptr<SystemShadow> shadow_;

    raw_ptr<views::Label> error_label_ = nullptr;

    raw_ptr<views::Link> try_again_link_ = nullptr;
  };

  ActionButtonContainerView();
  ActionButtonContainerView(const ActionButtonContainerView&) = delete;
  ActionButtonContainerView& operator=(const ActionButtonContainerView&) =
      delete;
  ~ActionButtonContainerView() override;

  // Adds an action button to the container. Returns a pointer to the added
  // button.
  // TODO(crbug.com/372740410): Determine behavior when we add a button with the
  // exact same rank (type and priority) as an existing valid button.
  ActionButtonView* AddActionButton(views::Button::PressedCallback callback,
                                    std::u16string text,
                                    const gfx::VectorIcon* icon,
                                    ActionButtonRank rank,
                                    ActionButtonViewID id);

  // Returns the action buttons in this container.
  const views::View::Views& GetActionButtons() const;

  // Returns all visible focusable views in this container, which may include
  // action buttons and the try again link in the error view.
  std::vector<views::View*> GetFocusableViews();

  // Removes and destroys all action buttons from this container. Also hides the
  // error view if it was visible.
  void ClearContainer();

  // Shows an error view with the given `error_message`. If `try_again_callback`
  // is not null, then the error view will also show a try again link that runs
  // `try_again_callback` when pressed.
  void ShowErrorView(
      const std::u16string& error_message,
      base::RepeatingClosure try_again_callback = base::NullCallback());

  // Hides the error view.
  void HideErrorView();

  // Starts performing the button transition triggered after pressing the smart
  // actions button. This will fade out existing action buttons, remove the
  // smart actions button, then animate in new icon buttons to replace the old
  // copy text and search buttons.
  void StartSmartActionsButtonTransition();

  // Removes the smart actions button. Called after the user declines the
  // disclaimer shown by the smart actions button.
  void RemoveSmartActionsButton();

  ErrorView* error_view_for_testing() { return error_view_; }

 private:
  // Called when the smart actions button has faded out, to start the transition
  // to new buttons. See `StartSmartActionsButtonTransition()`.
  void OnSmartActionsButtonFadedOut();

  // Enables or disables events on the action button container widget.
  void SetWidgetEventsEnabled(bool enabled);

  // Contains the row of action buttons.
  raw_ptr<views::View> action_button_row_ = nullptr;

  // Used to show an error message.
  raw_ptr<ErrorView> error_view_ = nullptr;

  base::WeakPtrFactory<ActionButtonContainerView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_ACTION_BUTTON_CONTAINER_VIEW_H_
