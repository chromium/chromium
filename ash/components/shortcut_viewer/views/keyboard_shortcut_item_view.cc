// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_view.h"

#include <memory>
#include <vector>

#include "ash/components/shortcut_viewer/keyboard_shortcut_item.h"
#include "ash/components/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/components/shortcut_viewer/vector_icons/vector_icons.h"
#include "ash/components/shortcut_viewer/views/bubble_view.h"
#include "ash/components/strings/grit/ash_components_strings.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"

namespace keyboard_shortcut_viewer {

namespace {

// Creates the separator view between bubble views of modifiers and key.
std::unique_ptr<views::View> CreateSeparatorView() {
  constexpr SkColor kSeparatorColor = SkColorSetARGB(0xFF, 0x1A, 0x73, 0xE8);
  constexpr int kIconSize = 16;

  std::unique_ptr<views::ImageView> separator_view =
      std::make_unique<views::ImageView>();
  separator_view->SetImage(
      gfx::CreateVectorIcon(kKsvSeparatorPlusIcon, kSeparatorColor));
  separator_view->SetImageSize(gfx::Size(kIconSize, kIconSize));
  separator_view->set_owned_by_client();
  return separator_view;
}

// Creates the bubble view for modifiers and key.
std::unique_ptr<views::View> CreateBubbleView(const base::string16& bubble_text,
                                              ui::KeyboardCode key_code) {
  auto bubble_view = std::make_unique<BubbleView>();
  bubble_view->set_owned_by_client();
  const gfx::VectorIcon* vector_icon = GetVectorIconForKeyboardCode(key_code);
  if (vector_icon)
    bubble_view->SetIcon(*vector_icon);
  else
    bubble_view->SetText(bubble_text);
  return bubble_view;
}

}  // namespace

KeyboardShortcutItemView::KeyboardShortcutItemView(
    const KeyboardShortcutItem& item,
    ShortcutCategory category)
    : shortcut_item_(&item), category_(category) {
  description_label_view_ = new views::StyledLabel(
      l10n_util::GetStringUTF16(item.description_message_id), nullptr);
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |description_label_view_| always
  // align to left.
  description_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
  AddChildView(description_label_view_);

  std::vector<size_t> offsets;
  std::vector<base::string16> replacement_strings;
  const size_t shortcut_key_codes_size = item.shortcut_key_codes.size();
  offsets.reserve(shortcut_key_codes_size);
  replacement_strings.reserve(shortcut_key_codes_size);
  bool has_invalid_dom_key = false;
  for (ui::KeyboardCode key_code : item.shortcut_key_codes) {
    auto iter = GetKeycodeToString16Cache()->find(key_code);
    if (iter == GetKeycodeToString16Cache()->end()) {
      iter = GetKeycodeToString16Cache()
                 ->emplace(key_code, GetStringForKeyboardCode(key_code))
                 .first;
    }
    const base::string16& dom_key_string = iter->second;
    // If the |key_code| has no mapped |dom_key_string|, we use alternative
    // string to indicate that the shortcut is not supported by current keyboard
    // layout.
    if (dom_key_string.empty()) {
      replacement_strings.clear();
      has_invalid_dom_key = true;
      break;
    }
    replacement_strings.emplace_back(dom_key_string);
  }

  base::string16 shortcut_string;
  if (replacement_strings.empty()) {
    shortcut_string = l10n_util::GetStringUTF16(has_invalid_dom_key
                                                    ? IDS_KSV_KEY_NO_MAPPING
                                                    : item.shortcut_message_id);
  } else {
    shortcut_string = l10n_util::GetStringFUTF16(item.shortcut_message_id,
                                                 replacement_strings, &offsets);
  }
  shortcut_label_view_ = new views::StyledLabel(shortcut_string, nullptr);
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |shortcut_label_view_| always
  // align to right.
  shortcut_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_LEFT : gfx::ALIGN_RIGHT);
  DCHECK_EQ(replacement_strings.size(), offsets.size());
  // TODO(wutao): make this reliable.
  // If the replacement string is "+ ", it indicates to insert a seperator view.
  const base::string16 separator_string = base::ASCIIToUTF16("+ ");
  for (size_t i = 0; i < offsets.size(); ++i) {
    views::StyledLabel::RangeStyleInfo style_info;
    style_info.disable_line_wrapping = true;
    const base::string16& replacement_string = replacement_strings[i];
    std::unique_ptr<views::View> custom_view =
        replacement_string == separator_string
            ? CreateSeparatorView()
            : CreateBubbleView(replacement_string, item.shortcut_key_codes[i]);
    style_info.custom_view = custom_view.get();
    shortcut_label_view_->AddCustomView(std::move(custom_view));
    shortcut_label_view_->AddStyleRange(
        gfx::Range(offsets[i], offsets[i] + replacement_strings[i].length()),
        style_info);
  }
  AddChildView(shortcut_label_view_);

  constexpr int kVerticalPadding = 10;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kVerticalPadding, 0, kVerticalPadding, 0)));

  // Use leaf list item role so that name is spoken by screen reader, but
  // redundant child label text is not also spoken.
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kListItem);
  GetViewAccessibility().OverrideIsLeaf();
  accessible_name_ = description_label_view_->text() +
                     base::ASCIIToUTF16(", ") + shortcut_label_view_->text();
}

void KeyboardShortcutItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
}

int KeyboardShortcutItemView::GetHeightForWidth(int w) const {
  MaybeCalculateAndDoLayout(w);
  return calculated_size_.height();
}

void KeyboardShortcutItemView::Layout() {
  MaybeCalculateAndDoLayout(GetLocalBounds().width());
}

// static
void KeyboardShortcutItemView::ClearKeycodeToString16Cache() {
  GetKeycodeToString16Cache()->clear();
}

// static
std::map<ui::KeyboardCode, base::string16>*
KeyboardShortcutItemView::GetKeycodeToString16Cache() {
  static base::NoDestructor<std::map<ui::KeyboardCode, base::string16>>
      key_code_to_string16_cache;
  return key_code_to_string16_cache.get();
}

void KeyboardShortcutItemView::MaybeCalculateAndDoLayout(int width) const {
  if (width == calculated_size_.width())
    return;
  TRACE_EVENT0("shortcut_viewer", "MaybeCalculateAndDoLayout");

  const gfx::Insets insets = GetInsets();
  width -= insets.width();
  if (width <= 0)
    return;

  // The max width of |shortcut_label_view_| as a ratio of its parent view's
  // width. This value is chosen to put all the bubble views in one line.
  constexpr float kShortcutViewPreferredWidthRatio = 0.69f;
  // The minimum spacing between |description_label_view_| and
  // |shortcut_label_view_|.
  constexpr int kMinimumSpacing = 64;

  const int shortcut_view_preferred_width =
      width * kShortcutViewPreferredWidthRatio;
  const int shortcut_view_height =
      shortcut_label_view_->GetHeightForWidth(shortcut_view_preferred_width);

  // Sets the bounds and layout in order to get the left most label in the
  // |shortcut_label_view_|, which is used to calculate the preferred width for
  // |description_label_view_|.
  shortcut_label_view_->SetBounds(0, 0, shortcut_view_preferred_width,
                                  shortcut_view_height);
  DCHECK(shortcut_label_view_->has_children());
  // Labels in |shortcut_label_view_| are right aligned, so we need to find the
  // minimum left coordinates of all the lables.
  int min_left = shortcut_view_preferred_width;
  for (int i = 0; i < shortcut_label_view_->child_count(); ++i) {
    min_left =
        std::min(min_left, shortcut_label_view_->child_at(i)->bounds().x());
  }

  // The width of |description_label_view_| will be dynamically adjusted to fill
  // the spacing.
  int description_view_preferred_width =
      width - (shortcut_view_preferred_width - min_left) - kMinimumSpacing;
  if (description_view_preferred_width < kMinimumSpacing) {
    // The min width of |description_label_view_| as a ratio of its parent
    // view's width when the |description_view_preferred_width| calculated above
    // is smaller than |kMinimumSpacing|.
    constexpr float kDescriptionViewMinWidthRatio = 0.29f;
    description_view_preferred_width = width * kDescriptionViewMinWidthRatio;
  }

  const int description_view_height =
      description_label_view_->GetHeightForWidth(
          description_view_preferred_width);

  // Sets the bounds and layout in order to get the center points of the views
  // making up the top lines in both the description and shortcut views.
  // We want the center of the top lines in both views to align with each other.
  description_label_view_->SetBounds(0, 0, description_view_preferred_width,
                                     description_view_height);
  DCHECK(shortcut_label_view_->has_children() &&
         description_label_view_->has_children());
  const int description_view_top_line_center_offset_y =
      description_label_view_->child_at(0)->bounds().CenterPoint().y();
  const int shortcut_view_top_line_center_offset_y =
      shortcut_label_view_->child_at(0)->bounds().CenterPoint().y();
  // |shortcut_label_view_| could have bubble view in the top line, whose
  // height is larger than normal text in |description_label_view_|. Otherwise,
  // the top line height in the two views should be equal.
  DCHECK_GE(shortcut_view_top_line_center_offset_y,
            description_view_top_line_center_offset_y);
  const int description_delta_y = shortcut_view_top_line_center_offset_y -
                                  description_view_top_line_center_offset_y;

  // Center align the top line in the two views.
  const int left = insets.left();
  const int top = insets.top();
  // Left align the |description_label_view_|.
  description_label_view_->SetBoundsRect(
      gfx::Rect(left, top + description_delta_y,
                description_view_preferred_width, description_view_height));
  // Right align the |shortcut_label_view_|.
  shortcut_label_view_->SetBoundsRect(
      gfx::Rect(left + width - shortcut_view_preferred_width, top,
                shortcut_view_preferred_width, shortcut_view_height));
  // Add 2 * |description_delta_y| to balance the top and bottom paddings.
  const int total_height =
      std::max(shortcut_view_height,
               description_view_height + 2 * description_delta_y) +
      insets.height();
  calculated_size_ = gfx::Size(width + insets.width(), total_height);
}

}  // namespace keyboard_shortcut_viewer
