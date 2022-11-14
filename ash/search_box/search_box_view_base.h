// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_
#define ASH_SEARCH_BOX_SEARCH_BOX_VIEW_BASE_H_

#include <string>
#include <vector>

#include "ash/search_box/search_box_constants.h"
#include "base/bind.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class BoxLayoutView;
class ImageView;
class Label;
class Textfield;
}  // namespace views

namespace ash {

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
  SearchBoxViewBase();

  SearchBoxViewBase(const SearchBoxViewBase&) = delete;
  SearchBoxViewBase& operator=(const SearchBoxViewBase&) = delete;

  ~SearchBoxViewBase() override;

  // Creates the search box close button at the right edge of the search box.
  // The close button will initially be hidden. The visibility will be updated
  // appropriatelly when `UpdateButtonsVisibility()` gets called.
  views::ImageButton* CreateCloseButton(
      const base::RepeatingClosure& button_callback);

  // Creates the search box assistant button at the right edge of the search
  // box. Note that the assistant button will only be shown if close button is
  // hidden, as the buttons have the same expected position within the search
  // box.
  // The assistant button will initially be hidden. The visibility will be
  // updated appropriatelly when `UpdateButtonsVisibility()` gets called.
  views::ImageButton* CreateAssistantButton(
      const base::RepeatingClosure& button_callback);

  bool HasSearch() const;

  // Returns the bounds to use for the view (including the shadow) given the
  // desired bounds of the search box contents.
  gfx::Rect GetViewBoundsForSearchBoxContentsBounds(
      const gfx::Rect& rect) const;

  views::ImageButton* assistant_button();
  views::ImageButton* close_button();
  views::ImageView* search_icon();
  views::Textfield* search_box() { return search_box_; }

  // Called when the query in the search box textfield changes. The search box
  // implementation is expected to handle the new query.
  // `query` the new search box query.
  // `initiated_by_user` whether the query changes was a result of the user
  // typing.
  virtual void HandleQueryChange(const std::u16string& query,
                                 bool initiated_by_user) = 0;

  // Sets contents for the title and category labels used for ghost text
  // autocomplete.
  void MaybeSetAutocompleteGhostText(const std::u16string& title,
                                     const std::u16string& category);

  std::string GetSearchBoxGhostTextForTest();

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
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnThemeChanged() override;

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

  void ClearSearch();

  // Called when the search box active state changes.
  virtual void OnSearchBoxActiveChanged(bool active);

  // Updates the painting if the focus moves to or from the search box.
  virtual void UpdateSearchBoxFocusPaint();

 protected:
  struct InitParams {
    InitParams();
    ~InitParams();
    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;

    // Whether to show close button if the search box is active and empty.
    bool show_close_button_when_active = false;

    // Whether to create a rounded-rect background.
    bool create_background = true;

    // Whether to animate the transition when the search icon is changed.
    bool animate_changing_search_icon = false;

    // Whether we should increase spacing between `search_icon_', 'search_box_',
    // and the 'search_box_button_container_'.
    bool increase_child_view_padding = false;

    // If set, the margins that should be used for the search box text field.
    absl::optional<gfx::Insets> textfield_margins;
  };

  void Init(const InitParams& params);

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

  views::BoxLayoutView* box_layout_view() { return content_container_; }

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

  // Updates the color and alignment of the placeholder text.
  virtual void UpdatePlaceholderTextStyle() {}

  // Update search box border based on whether the search box is activated.
  virtual void UpdateSearchBoxBorder() {}

  // Records in histograms the activation of the searchbox.
  virtual void RecordSearchBoxActivationHistogram(ui::EventType event_type) {}

 private:
  void OnEnabledChanged();

  // Owned by views hierarchy.
  views::BoxLayoutView* content_container_;
  SearchIconImageView* search_icon_ = nullptr;
  SearchBoxImageButton* assistant_button_ = nullptr;
  SearchBoxImageButton* close_button_ = nullptr;
  views::BoxLayoutView* text_container_ = nullptr;

  views::Textfield* search_box_;
  views::BoxLayoutView* ghost_text_container_ = nullptr;
  views::Label* separator_label_ = nullptr;
  views::Label* autocomplete_ghost_text_ = nullptr;
  views::Label* category_separator_label_ = nullptr;
  views::Label* category_ghost_text_ = nullptr;

  views::View* search_box_button_container_ = nullptr;

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
