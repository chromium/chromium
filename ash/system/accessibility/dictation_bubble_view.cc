// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_view.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/lottie/animation.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kIconSizeDip = 16;
constexpr int kSpaceBetweenTopRowAndDictationHintViewsDip = 4;
constexpr int kSpaceBetweenHintLabelsDip = 4;
constexpr int kSpaceBetweenIconAndTextDip = 4;
constexpr int kMaxNumHints = 5;

std::unique_ptr<views::ImageView> CreateImageView(
    raw_ptr<views::ImageView>* destination_view,
    const gfx::VectorIcon& icon) {
  return views::Builder<views::ImageView>()
      .CopyAddressTo(destination_view)
      .SetImage(ui::ImageModel::FromVectorIcon(icon, kColorAshTextColorPrimary,
                                               kIconSizeDip))
      .Build();
}

std::unique_ptr<views::Label> CreateLabelView(
    raw_ptr<views::Label>* destination_view,
    const std::u16string& text,
    ui::ColorId enabled_color_id) {
  return views::Builder<views::Label>()
      .CopyAddressTo(destination_view)
      .SetText(text)
      .SetEnabledColorId(enabled_color_id)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .Build();
}

int ToMessageId(DictationBubbleHintType hint_type) {
  switch (hint_type) {
    case DictationBubbleHintType::kTrySaying:
      return IDS_ASH_DICTATION_HINT_TRY_SAYING;
    case DictationBubbleHintType::kType:
      return IDS_ASH_DICTATION_HINT_TYPE;
    case DictationBubbleHintType::kDelete:
      return IDS_ASH_DICTATION_HINT_DELETE;
    case DictationBubbleHintType::kSelectAll:
      return IDS_ASH_DICTATION_HINT_SELECT_ALL;
    case DictationBubbleHintType::kUndo:
      return IDS_ASH_DICTATION_HINT_UNDO;
    case DictationBubbleHintType::kHelp:
      return IDS_ASH_DICTATION_HINT_HELP;
    case DictationBubbleHintType::kUnselect:
      return IDS_ASH_DICTATION_HINT_UNSELECT;
    case DictationBubbleHintType::kCopy:
      return IDS_ASH_DICTATION_HINT_COPY;
  }
}

// View for the Dictation bubble top row. Responsible for displaying icons,
// animations, and non-finalized speech results.
class ASH_EXPORT TopRowView : public views::View {
  METADATA_HEADER(TopRowView, views::View)

 public:
  TopRowView() {
    std::unique_ptr<views::BoxLayout> layout =
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal);
    layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
    SetLayoutManager(std::move(layout));

    AddChildView(CreateStandbyView());
    AddChildView(CreateImageView(&macro_succeeded_image_,
                                 kDictationBubbleMacroSucceededIcon));
    AddChildView(
        CreateImageView(&macro_failed_image_, kDictationBubbleMacroFailedIcon));
    AddChildView(
        CreateLabelView(&label_, std::u16string(), kColorAshTextColorPrimary));

    GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
    // Note: this static variable is used so that this view can be identified
    // from tests. Do not change this, as it will cause test failures.
    GetViewAccessibility().SetClassName("DictationBubbleView");
  }

  TopRowView(const TopRowView&) = delete;
  TopRowView& operator=(const TopRowView&) = delete;
  ~TopRowView() override = default;

  // Updates the visibility of all child views. Also updates the text content
  // of `label_` and updates the size of this view.
  void Update(DictationBubbleIconType icon,
              const std::optional<std::u16string>& text) {
    // Update visibility.
    bool is_standby = icon == DictationBubbleIconType::kStandby;
    if (use_standby_animation_) {
      standby_animation_->SetVisible(is_standby);
      is_standby ? standby_animation_->Play() : standby_animation_->Stop();
    } else {
      standby_image_->SetVisible(is_standby);
    }

    macro_succeeded_image_->SetVisible(icon ==
                                       DictationBubbleIconType::kMacroSuccess);
    macro_failed_image_->SetVisible(icon ==
                                    DictationBubbleIconType::kMacroFail);

    // Update label.
    label_->SetVisible(text.has_value());
    label_->SetText(text.has_value() ? text.value() : std::u16string());
    SizeToPreferredSize();
  }

 private:
  friend class ash::DictationBubbleView;

  // Returns a std::unique_ptr<AnimatedImageView> if the standby animation
  // can successfully be loaded. Otherwise, returns a std::unique_ptr<ImageView>
  // as a fallback.
  std::unique_ptr<views::View> CreateStandbyView() {
    std::optional<std::string> json =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_DICTATION_BUBBLE_ANIMATION);
    if (json.has_value()) {
      use_standby_animation_ = true;
      auto skottie = cc::SkottieWrapper::UnsafeCreateSerializable(
          std::vector<uint8_t>(json.value().begin(), json.value().end()));
      return views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&standby_animation_)
          .SetAnimatedImage(std::make_unique<lottie::Animation>(skottie))
          .SetImageSize(gfx::Size(18, 16))
          .Build();
    }

    use_standby_animation_ = false;
    return CreateImageView(&standby_image_, kDictationBubbleIcon);
  }

  // Owned by the views hierarchy.
  // An animation that is shown when Dictation is standing by.
  raw_ptr<views::AnimatedImageView> standby_animation_ = nullptr;
  // An image that is shown when Dictation is standing by. Only used if the
  // above AnimatedImageView fails to initialize.
  raw_ptr<views::ImageView> standby_image_ = nullptr;
  // If true, this view will use `standby_animation_`. Otherwise, will use
  // `standby_image_`.
  bool use_standby_animation_ = false;
  // An image that is shown when a macro is successfully run.
  raw_ptr<views::ImageView> macro_succeeded_image_ = nullptr;
  // An image that is shown when a macro fails to run.
  raw_ptr<views::ImageView> macro_failed_image_ = nullptr;
  // A label that displays non-final speech results.
  raw_ptr<views::Label> label_ = nullptr;
};

BEGIN_METADATA(TopRowView)
END_METADATA

}  // namespace

DictationBubbleView::DictationBubbleView() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_parent_window(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
}

DictationBubbleView::~DictationBubbleView() = default;

void DictationBubbleView::Update(
    DictationBubbleIconType icon,
    const std::optional<std::u16string>& text,
    const std::optional<std::vector<DictationBubbleHintType>>& hints) {
  top_row_view_->Update(icon, text);
  hint_view_->Update(hints);
  SizeToContents();
}

void DictationBubbleView::Init() {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(
      kSpaceBetweenTopRowAndDictationHintViewsDip);
  SetLayoutManager(std::move(layout));
  UseCompactMargins();

  top_row_view_ = AddChildView(std::make_unique<TopRowView>());
  hint_view_ = AddChildView(std::make_unique<DictationHintView>());
}

void DictationBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_BUBBLE;
  params->opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params->activatable = views::Widget::InitParams::Activatable::kNo;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->name = "DictationBubbleView";
}

std::u16string DictationBubbleView::GetTextForTesting() {
  return top_row_view_->label_->GetText();
}

bool DictationBubbleView::IsStandbyViewVisibleForTesting() {
  if (top_row_view_->use_standby_animation_)
    return top_row_view_->standby_animation_->GetVisible();

  return top_row_view_->standby_image_->GetVisible();
}

bool DictationBubbleView::IsMacroSucceededImageVisibleForTesting() {
  return top_row_view_->macro_succeeded_image_->GetVisible();
}

bool DictationBubbleView::IsMacroFailedImageVisibleForTesting() {
  return top_row_view_->macro_failed_image_->GetVisible();
}

SkColor DictationBubbleView::GetLabelBackgroundColorForTesting() {
  return top_row_view_->label_->GetBackgroundColor();
}

SkColor DictationBubbleView::GetLabelTextColorForTesting() {
  return top_row_view_->label_->GetEnabledColor();
}

std::vector<std::u16string> DictationBubbleView::GetVisibleHintsForTesting() {
  std::vector<std::u16string> hints;
  for (size_t i = 0; i < hint_view_->labels_.size(); ++i) {
    views::Label* label = hint_view_->labels_[i];
    if (label->GetVisible())
      hints.push_back(label->GetText());
  }
  return hints;
}

views::View* DictationBubbleView::GetTopRowView() {
  return top_row_view_;
}

BEGIN_METADATA(DictationBubbleView)
END_METADATA

DictationHintView::DictationHintView() {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical);
  layout->set_between_child_spacing(kSpaceBetweenHintLabelsDip);
  SetLayoutManager(std::move(layout));

  for (size_t i = 0; i < labels_.size(); ++i) {
    // The first label should use the secondary text color. All other labels
    // should use the primary text color.
    ui::ColorId color_id =
        i == 0 ? kColorAshTextColorSecondary : kColorAshTextColorPrimary;
    AddChildView(CreateLabelView(&labels_[i], std::u16string(), color_id));
  }

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
}

DictationHintView::~DictationHintView() = default;

void DictationHintView::Update(
    const std::optional<std::vector<DictationBubbleHintType>>& hints) {
  int num_visible_hints = 0;
  if (hints.has_value()) {
    DCHECK(hints.value().size() <= kMaxNumHints);
    num_visible_hints = hints.value().size();
  }

  // Update labels.
  for (size_t i = 0; i < labels_.size(); ++i) {
    bool has_hint_for_index = hints.has_value() && (i < hints.value().size());
    labels_[i]->SetVisible(has_hint_for_index);
    if (has_hint_for_index) {
      labels_[i]->SetText(
          l10n_util::GetStringUTF16(ToMessageId(hints.value()[i])));
    } else {
      labels_[i]->SetText(std::u16string());
    }
  }

  // Set visibility of this view based on the number of visible hints.
  // If the hint view is visible, send an alert event so that ChromeVox reads
  // hints to the user.
  if (num_visible_hints > 0) {
    SetVisible(true);
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  } else {
    SetVisible(false);
  }
  SizeToPreferredSize();
}

BEGIN_METADATA(DictationHintView)
END_METADATA

}  // namespace ash
