// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/style/counter_expand_button.h"
#include "ash/style/error_message_toast.h"
#include "base/functional/callback_forward.h"
#include "ui/base/models/combobox_model.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class Combobox;
class GlanceablesContentsScrollView;
class GlanceablesListFooterView;
class GlanceablesProgressBarView;

// Glanceables Time Management bubble container that is a child of
// `GlanceableTrayChildBubble`.
class ASH_EXPORT GlanceablesTimeManagementBubbleView
    : public views::FlexLayoutView,
      public gfx::AnimationDelegate {
  METADATA_HEADER(GlanceablesTimeManagementBubbleView, views::FlexLayoutView)

 public:
  // The attribute that describes what type this view is used for.
  // Note that the enum values should not be reordered or reused as the values
  // are used in prefs (kGlanceablesTimeManagementLastExpandedBubble).
  enum class Context { kTasks = 0, kClassroom = 1 };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the bubble view expand state change to `is_expanded`.
    // `expand_by_overscroll` is set to true if the bubble view is expanded by
    // overscroll. Details can be found in `GlanceablesContentsScrollView`.
    virtual void OnExpandStateChanged(Context context,
                                      bool is_expanded,
                                      bool expand_by_overscroll) = 0;
  };

  struct InitParams {
    InitParams();
    InitParams(InitParams&& other);
    ~InitParams();

    Context context;
    std::unique_ptr<ui::ComboboxModel> combobox_model;
    std::u16string combobox_tooltip;
    int expand_button_tooltip_id = 0;
    int collapse_button_tooltip_id = 0;
    std::u16string footer_title;
    std::u16string footer_tooltip;
    raw_ptr<const gfx::VectorIcon> header_icon = nullptr;
    int header_icon_tooltip_id = 0;
  };

  explicit GlanceablesTimeManagementBubbleView(InitParams params);
  GlanceablesTimeManagementBubbleView(
      const GlanceablesTimeManagementBubbleView&) = delete;
  GlanceablesTimeManagementBubbleView& operator=(
      const GlanceablesTimeManagementBubbleView&) = delete;
  ~GlanceablesTimeManagementBubbleView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void Layout(PassKey) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Creates `this` view's own background and updates layout accordingly.
  void CreateElevatedBackground();

  // Updates `is_expanded_` and the view layout.
  // `expand_by_overscroll` indicates whether the expand state change is
  // triggered by overscroll. More information can be found in
  // `GlanceablesContentsScrollView` class.
  void SetExpandState(bool is_expanded, bool expand_by_overscroll);

  // Returns the preferred height of `this` in the collapsed state. This is used
  // to calculate the available size for glanceables. This should be constant
  // after the view is laid out.
  int GetCollapsedStatePreferredHeight() const;

  // Returns the expanded/collapsed state of the bubble view.
  bool IsExpanded() const { return is_expanded_; }

  bool is_animating_resize() const {
    return resize_animation_ && resize_animation_->is_animating();
  }

  void SetAnimationEndedClosureForTest(base::OnceClosure closure);

 protected:
  class GlanceablesExpandButton : public CounterExpandButton {
    METADATA_HEADER(GlanceablesExpandButton, CounterExpandButton)
   public:
    GlanceablesExpandButton();
    ~GlanceablesExpandButton() override;

    void SetExpandedStateTooltipStringId(int tooltip_text_id);
    void SetCollapsedStateTooltipStringId(int tooltip_text_id);
    std::u16string GetExpandedStateTooltipText() const override;
    std::u16string GetCollapsedStateTooltipText() const override;

   private:
    // The tooltip string that tells that the button can expand the bubble.
    int expand_tooltip_string_id_ = 0;
    // The tooltip string that tells that the button can collapse the bubble.
    int collapse_tooltip_string_id_ = 0;
  };

  // Linear animation to track time management bubble resize animation - as the
  // animation progresses, the bubble view preferred size will change causing
  // bubble bounds updates. `ResizeAnimation` will provide the expected
  // preferred time management bubble height.
  class ResizeAnimation : public gfx::LinearAnimation {
   public:
    // The context of the animation that determines the type of tweens and
    // duration to use.
    enum class Type {
      kContainerExpandStateChanged,
      kChildResize,
    };

    ResizeAnimation(int start_height,
                    int end_height,
                    gfx::AnimationDelegate* delegate,
                    Type type);

    int GetCurrentHeight() const;

   private:
    const Type type_;

    const int start_height_;
    const int end_height_;
  };

  // Handles press on the header icon button in `header_view_`.
  virtual void OnHeaderIconPressed() = 0;

  // Handles press on the "See all" button in `GlanceablesListFooterView`.
  virtual void OnFooterButtonPressed() = 0;

  // Called when the selected list changed in `combobox_view_`.
  virtual void SelectedListChanged() = 0;

  // Triggers bubble view resize animation to new preferred size, if an
  // animation is required.
  virtual void AnimateResize(ResizeAnimation::Type resize_type) = 0;

  // Updates the interior margin according to the current view state.
  void UpdateInteriorMargin();

  // Creates and initializes `combobox_view_`.
  void CreateComboBoxView();

  // Returns the index that `combobox_view_` is currently selected.
  size_t GetComboboxSelectedIndex() const;

  // Updates the text on `combobox_replacement_label_`.
  void UpdateComboboxReplacementLabelText();

  void SetUpResizeThroughputTracker(const std::string& histogram_name);

  // Removes an active `error_message_` from the view, if any.
  void MaybeDismissErrorMessage();
  void ShowErrorMessage(const std::u16string& error_message,
                        views::Button::PressedCallback callback,
                        ErrorMessageToast::ButtonActionType type);

  Combobox* combobox_view() { return combobox_view_; }
  GlanceablesExpandButton* expand_button() { return expand_button_; }
  GlanceablesProgressBarView* progress_bar() { return progress_bar_; }
  GlanceablesContentsScrollView* content_scroll_view() {
    return content_scroll_view_;
  }
  views::View* items_container_view() { return items_container_view_; }
  GlanceablesListFooterView* list_footer_view() { return list_footer_view_; }

  ui::ComboboxModel* combobox_model() { return combobox_model_.get(); }

  // Linear animation that drive time management bubble resize animation - the
  // animation updates the time management bubble view preferred size, which
  // causes layout updates. Runs when the bubble preferred size changes.
  std::unique_ptr<ResizeAnimation> resize_animation_;

  base::ObserverList<Observer> observers_;

 private:
  // Toggles `is_expanded_` and updates the layout.
  void ToggleExpandState();

  const Context context_;

  // Whether the view is expanded and showing the contents in contents scroll
  // view.
  bool is_expanded_ = true;

  // Owned by views hierarchy.
  raw_ptr<views::FlexLayoutView> header_view_ = nullptr;
  raw_ptr<Combobox> combobox_view_ = nullptr;
  // This is a simple label that copies the label style on `combo_box_view_` so
  // that it can visually replace it when `combo_box_view_` is hidden.
  raw_ptr<views::Label> combobox_replacement_label_ = nullptr;
  raw_ptr<GlanceablesExpandButton> expand_button_ = nullptr;
  raw_ptr<GlanceablesProgressBarView> progress_bar_ = nullptr;
  raw_ptr<GlanceablesContentsScrollView> content_scroll_view_ = nullptr;
  raw_ptr<views::View> items_container_view_ = nullptr;
  raw_ptr<GlanceablesListFooterView> list_footer_view_ = nullptr;

  // The model for `combobox_view_`.
  const std::unique_ptr<ui::ComboboxModel> combobox_model_;

  // Measure animation smoothness metrics for `resize_animation_`.
  std::optional<ui::ThroughputTracker> resize_throughput_tracker_;

  // Called when `resize_animation_` ends or is canceled. This is currently only
  // used in test.
  base::OnceClosure resize_animation_ended_closure_;

  // Owned by views hierarchy.
  raw_ptr<ErrorMessageToast> error_message_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_
