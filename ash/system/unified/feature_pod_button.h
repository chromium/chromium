// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {

class FeaturePodControllerBase;

// TODO(crbug.com/40808951): Remove FeaturePodIconButton after the migration.
// A toggle button with an icon used by feature pods and in other places.
class ASH_EXPORT FeaturePodIconButton : public IconButton {
  METADATA_HEADER(FeaturePodIconButton, IconButton)

 public:
  FeaturePodIconButton(PressedCallback callback, bool is_togglable);
  FeaturePodIconButton(const FeaturePodIconButton&) = delete;
  FeaturePodIconButton& operator=(const FeaturePodIconButton&) = delete;
  ~FeaturePodIconButton() override;
};

// Button internally used in FeaturePodButton. Should not be used directly.
class ASH_EXPORT FeaturePodLabelButton : public views::Button {
  METADATA_HEADER(FeaturePodLabelButton, views::Button)

 public:
  explicit FeaturePodLabelButton(PressedCallback callback);

  FeaturePodLabelButton(const FeaturePodLabelButton&) = delete;
  FeaturePodLabelButton& operator=(const FeaturePodLabelButton&) = delete;

  ~FeaturePodLabelButton() override;

  // Set the text of label shown below the icon. See FeaturePodButton::SetLabel.
  void SetLabel(const std::u16string& label);
  const std::u16string& GetLabelText() const;

  // Set the text of sub-label shown below the label.
  // See FeaturePodButton::SetSubLabel.
  void SetSubLabel(const std::u16string& sub_label);
  const std::u16string& GetSubLabelText() const;

  // Show arrow to indicate that the feature has a detailed view.
  // See FeaturePodButton::ShowDetailedViewArrow.
  void ShowDetailedViewArrow();

  // views::Button:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

 private:
  // Layout |child| in horizontal center with its vertical origin set to |y|.
  void LayoutInCenter(views::View* child, int y);

  // views::Button:
  void OnEnabledChanged() override;

  // Owned by views hierarchy.
  const raw_ptr<views::Label> label_;
  const raw_ptr<views::Label> sub_label_;
  const raw_ptr<views::ImageView> detailed_view_arrow_;
};

// A button in FeaturePodsView. These buttons are main entry points of features
// in UnifiedSystemTray. Each button has its icon, label, and sub-label placed
// vertically. The button may be togglable and the background color indicates
// the current state. Otherwise, the button is not a toggle button and just
// navigates to the appropriate detailed view.
// See the comment in FeaturePodsView for detail.
class ASH_EXPORT FeaturePodButton : public views::View {
  METADATA_HEADER(FeaturePodButton, views::View)

 public:
  explicit FeaturePodButton(FeaturePodControllerBase* controller,
                            bool is_togglable = true);

  FeaturePodButton(const FeaturePodButton&) = delete;
  FeaturePodButton& operator=(const FeaturePodButton&) = delete;

  ~FeaturePodButton() override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Set the text of label shown below the icon.
  void SetLabel(const std::u16string& label);

  // Set the text of sub-label shown below the label.
  void SetSubLabel(const std::u16string& sub_label);

  // Set the tooltip text of the icon button.
  void SetIconTooltip(const std::u16string& text);

  // Set the tooltip text of the label button.
  void SetLabelTooltip(const std::u16string& text);

  // Convenience method to set both icon and label tooltip texts.
  void SetIconAndLabelTooltips(const std::u16string& text);

  // Show arrow to indicate that the feature has a detailed view.
  void ShowDetailedViewArrow();

  // Remove the label button from keyboard focus chain. This is useful when
  // the icon button and the label button has the same action.
  void DisableLabelButtonFocus();

  // Change the toggled state. If toggled, the background color of the circle
  // will change. If the button is not togglable, then SetToggled() will do
  // nothing and |IsToggled()| will always return false.
  void SetToggled(bool toggled);
  bool IsToggled() const { return icon_button_->toggled(); }

  // Change the expanded state. 0.0 if collapsed, and 1.0 if expanded.
  // Otherwise, it shows intermediate state. In the collapsed state, the labels
  // are not shown, so the label buttons always fade out as expanded_amount
  // decreases. We also need to fade out the icon button when it's not part of
  // the buttons visible in the collapsed state. fade_icon_button will be passed
  // as true for these cases.
  void SetExpandedAmount(double expanded_amount, bool fade_icon_button);

  // Get opacity for a given expanded_amount value. Used to fade out
  // all label buttons and icon buttons that are hidden in collapsed state
  // while collapsing.
  double GetOpacityForExpandedAmount(double expanded_amount);

  // Only called by the container. Same as SetVisible but doesn't change
  // |visible_preferred_| flag.
  void SetVisibleByContainer(bool visible);

  // views::View:
  void SetVisible(bool visible) override;
  bool HasFocus() const override;
  void RequestFocus() override;

  bool visible_preferred() const { return visible_preferred_; }

  FeaturePodIconButton* icon_button() const { return icon_button_; }
  FeaturePodLabelButton* label_button() const { return label_button_; }

 private:
  // For unit tests.
  friend class BluetoothFeaturePodControllerTest;
  friend class NetworkFeaturePodControllerTest;
  friend class NightLightFeaturePodControllerTest;

  void OnEnabledChanged();

  // Owned by views hierarchy.
  const raw_ptr<FeaturePodIconButton> icon_button_;
  const raw_ptr<FeaturePodLabelButton> label_button_;

  // If true, it is preferred by the FeaturePodController that the view is
  // visible. Usually, this should match visible(), but in case that the
  // container does not have enough space, it might not match.
  // In such case, the preferred visibility is reflected after the container is
  // expanded.
  bool visible_preferred_ = true;

  base::CallbackListSubscription enabled_changed_subscription_ =
      AddEnabledChangedCallback(
          base::BindRepeating(&FeaturePodButton::OnEnabledChanged,
                              base::Unretained(this)));
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_BUTTON_H_
