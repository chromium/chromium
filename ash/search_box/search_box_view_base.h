// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_

#include <string>
#include <vector>

#include "ash/search_box/search_box_constants.h"
#include "base/bind.h"
#include "ui/events/types/event_type.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class BoxLayout;
class ImageView;
class Textfield;
}  // namespace views

namespace ash {

class SearchBoxViewDelegate;
class SearchBoxImageButton;
class SearchIconImageView;

// These are used in histograms, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// SearchBoxActivationSource enum listing in tools/metrics/histograms/enums.xml.
enum class ActivationSource {
  kMousePress = 0,
  kKeyPress = 1,
  kGestureTap = 2,
  kMaxValue = kGestureTap,
};

// SearchBoxViewBase consists of icons and a Textfield. The Textfiled is for
// inputting queries and triggering callbacks. The icons include a search icon,
// a close icon and a back icon for different functionalities. This class
// provides common functions for the search box view across Chrome OS.
class SearchBoxViewBase : public views::View,
                          public views::TextfieldController {
 public:
  explicit SearchBoxViewBase(SearchBoxViewDelegate* delegate);

  SearchBoxViewBase(const SearchBoxViewBase&) = delete;
  SearchBoxViewBase& operator=(const SearchBoxViewBase&) = delete;

  ~SearchBoxViewBase() override;

  struct InitParams {
    // Whether to show close button if the search box is active and empty.
    bool show_close_button_when_active = false;

    // Whether to create a rounded-rect background.
    bool create_background = true;

    // Whether to animate the transition when the search icon is changed.
    bool animate_changing_search_icon = false;

    // Whether we should increase spacing between `search_icon_', 'search_box_',
    // and the 'search_box_button_container_'.
    bool increase_child_view_padding = false;
  };
  virtual void Init(const InitParams& params);

  bool HasSearch() const;

  // Returns the bounds to use for the view (including the shadow) given the
  // desired bounds of the search box contents.
  gfx::Rect GetViewBoundsForSearchBoxContentsBounds(
      const gfx::Rect& rect) const;

  views::ImageButton* assistant_button();
  views::ImageButton* back_button();
  views::ImageButton* close_button();
  views::ImageView* search_icon();
  views::Textfield* search_box() { return search_box_; }

  // Swaps the google icon with the back button.
  void ShowBackOrGoogleIcon(bool show_back_button);

  // Setting the search box active left aligns the placeholder text, changes
  // the color of the placeholder text, and enables cursor blink. Setting the
  // search box inactive center aligns the placeholder text, sets the color, and
  // disables cursor blink.
  void SetSearchBoxActive(bool active, ui::EventType event_type);

  // Handles Gesture and Mouse Events sent from |search_box_|.
  bool OnTextfieldEvent(ui::EventType type);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Allows for search box to be notified of gestures occurring outside, without
  // deactivating the searchbox.
  void NotifyGestureEvent();

  // Whether the search box is active.
  bool is_search_box_active() const { return is_search_box_active_; }

  bool show_assistant_button() { return show_assistant_button_; }

  void OnSearchBoxFocusedChanged();

  // Whether the trimmed query in the search box is empty.
  bool IsSearchBoxTrimmedQueryEmpty() const;

  virtual void UpdateSearchTextfieldAccessibleNodeData(
      ui::AXNodeData* node_data);

  virtual void ClearSearch();

  // Called when the search box active state changes.
  virtual void OnSearchBoxActiveChanged(bool active);

  // Updates the painting if the focus moves to or from the search box.
  virtual void UpdateSearchBoxFocusPaint();

 protected:
  // Fires query change notification.
  void NotifyQueryChanged();

  // Nofifies the active status change.
  void NotifyActiveChanged();

  // Updates the visibility of the close and assistant buttons.
  void UpdateButtonsVisibility();

  // When necessary, starts the fade in animation for the button.
  void MaybeFadeButtonIn(SearchBoxImageButton* button);

  // When necessary, starts the fade out animation for the button.
  void MaybeFadeButtonOut(SearchBoxImageButton* button);

  // Used as a callback to set the button's visibility to false.
  void SetVisibilityHidden(SearchBoxImageButton* button);

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

  SearchBoxViewDelegate* delegate() { return delegate_; }
  views::BoxLayout* box_layout() { return box_layout_; }

  void SetSearchBoxBackgroundCornerRadius(int corner_radius);

  void SetSearchIconImage(gfx::ImageSkia image);

  void SetShowAssistantButton(bool show);

  // Detects |ET_MOUSE_PRESSED| and |ET_GESTURE_TAP| events on the white
  // background of the search box.
  virtual void HandleSearchBoxEvent(ui::LocatedEvent* located_event);

  // Updates the search box's background color.
  void UpdateBackgroundColor(SkColor color);

  // Shows/hides the virtual keyboard if the search box is active.
  virtual void UpdateKeyboardVisibility() {}

  // Updates model text and selection model with current Textfield info.
  virtual void UpdateModel(bool initiated_by_user) {}

  // Updates the search icon.
  virtual void UpdateSearchIcon() {}

  // Updates the color and alignment of the placeholder text.
  virtual void UpdatePlaceholderTextStyle() {}

  // Update search box border based on whether the search box is activated.
  virtual void UpdateSearchBoxBorder() {}

  // Setup button's image, accessible name, and tooltip text etc.
  virtual void SetupAssistantButton() {}
  virtual void SetupCloseButton() {}
  virtual void SetupBackButton() {}

  // Records in histograms the activation of the searchbox.
  virtual void RecordSearchBoxActivationHistogram(ui::EventType event_type) {}

 private:
  void OnEnabledChanged();

  SearchBoxViewDelegate* const delegate_;

  // Owned by views hierarchy.
  views::View* content_container_;
  SearchIconImageView* search_icon_ = nullptr;
  SearchBoxImageButton* assistant_button_ = nullptr;
  SearchBoxImageButton* back_button_ = nullptr;
  SearchBoxImageButton* close_button_ = nullptr;
  views::Textfield* search_box_;
  views::View* search_box_button_container_ = nullptr;

  // Owned by |content_container_|. It is deleted when the view is deleted.
  views::BoxLayout* box_layout_ = nullptr;

  // Whether the search box is active.
  bool is_search_box_active_ = false;
  // Whether to show close button if the search box is active and empty.
  bool show_close_button_when_active_ = false;
  // Whether to show assistant button.
  bool show_assistant_button_ = false;

  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&SearchBoxViewBase::OnEnabledChanged,
                              base::Unretained(this)));

  base::WeakPtrFactory<SearchBoxViewBase> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_
