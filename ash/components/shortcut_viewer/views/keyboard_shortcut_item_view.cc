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
  std::vector<base::string16> accessible_names;
  const size_t shortcut_key_codes_size = item.shortcut_key_codes.size();
  offsets.reserve(shortcut_key_codes_size);
  replacement_strings.reserve(shortcut_key_codes_size);
  accessible_names.reserve(shortcut_key_codes_size);
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
      accessible_names.clear();
      has_invalid_dom_key = true;
      break;
    }
    replacement_strings.emplace_back(dom_key_string);

    base::string16 accessible_name = GetAccessibleNameForKeyboardCode(key_code);
    accessible_names.emplace_back(accessible_name.empty() ? dom_key_string
                                                          : accessible_name);
  }

  base::string16 shortcut_string;
  base::string16 accessible_string;
  if (replacement_strings.empty()) {
    shortcut_string = l10n_util::GetStringUTF16(has_invalid_dom_key
                                                    ? IDS_KSV_KEY_NO_MAPPING
                                                    : item.shortcut_message_id);
    accessible_string = shortcut_string;
  } else {
    shortcut_string = l10n_util::GetStringFUTF16(item.shortcut_message_id,
                                                 replacement_strings, &offsets);
    accessible_string = l10n_util::GetStringFUTF16(
        item.shortcut_message_id, accessible_names, /*offsets=*/nullptr);
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
  GetViewAccessibility().OverrideIsLeaf(true);
  accessible_name_ = description_label_view_->GetText() +
                     base::ASCIIToUTF16(", ") + accessible_string;
}

void KeyboardShortcutItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
}

int KeyboardShortcutItemView::GetHeightForWidth(int w) const {
  CalculateLayout(w);
  return calculated_size_.height();
}

void KeyboardShortcutItemView::Layout() {
  CalculateLayout(width());
  description_label_view_->SetBoundsRect(description_bounds_);
  shortcut_label_view_->SetBoundsRect(shortcut_bounds_);
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

void KeyboardShortcutItemView::CalculateLayout(int width) const {
  if (width == calculated_size_.width())
    return;

  TRACE_EVENT0("shortcut_viewer", "CalculateLayout");

  const gfx::Insets insets = GetInsets();
  const int content_width = std::max(width - insets.width(), 0);

  // The max width of |shortcut_label_view_| as a ratio of its parent view's
  // width. This value is chosen to put all the bubble views in one line.
  constexpr float kShortcutViewPreferredWidthRatio = 0.69f;
  // The minimum spacing between |description_label_view_| and
  // |shortcut_label_view_|.
  constexpr int kMinimumSpacing = 64;

  const int shortcut_width = content_width * kShortcutViewPreferredWidthRatio;
  const auto& shortcut_size_info =
      shortcut_label_view_->GetLayoutSizeInfoForWidth(shortcut_width);
  const int shortcut_height = shortcut_size_info.total_size.height();
  const auto top_line_height = [](const auto& size_info) {
    // When nothing fits, it doesn't really matter what we do; using the overall
    // height (which is the height of the label insets) is sane.
    return size_info.line_sizes.empty() ? size_info.total_size.height()
                                        : size_info.line_sizes[0].height();
  };
  const int shortcut_top_line_center_y =
      shortcut_label_view_->GetInsets().top() +
      (top_line_height(shortcut_size_info) / 2);

  // The width of |description_label_view_| will be dynamically adjusted to fill
  // the spacing.
  int description_width =
      content_width - shortcut_size_info.total_size.width() - kMinimumSpacing;
  if (description_width < kMinimumSpacing) {
    // The min width of |description_label_view_| as a ratio of its parent
    // view's width when the |description_view_width| calculated above is
    // smaller than |kMinimumSpacing|.
    constexpr float kDescriptionViewMinWidthRatio = 0.29f;
    description_width = content_width * kDescriptionViewMinWidthRatio;
  }
  const auto& description_size_info =
      description_label_view_->GetLayoutSizeInfoForWidth(description_width);
  const int description_height = description_size_info.total_size.height();
  const int description_top_line_center_y =
      description_label_view_->GetInsets().top() +
      (top_line_height(description_size_info) / 2);

  // |shortcut_label_view_| could have bubble view in the top line, whose
  // height is larger than normal text in |description_label_view_|. Otherwise,
  // the top line height in the two views should be equal.
  DCHECK_GE(shortcut_top_line_center_y, description_top_line_center_y);

  // Align the vertical center of the top lines of both views.
  const int description_delta_y =
      shortcut_top_line_center_y - description_top_line_center_y;

  // Left align the |description_label_view_|.
  description_bounds_ =
      gfx::Rect(insets.left(), insets.top() + description_delta_y,
                description_width, description_height);
  // Right align the |shortcut_label_view_|.
  shortcut_bounds_ = gfx::Rect(insets.left() + content_width - shortcut_width,
                               insets.top(), shortcut_width, shortcut_height);
  // Add 2 * |description_delta_y| to balance the top and bottom paddings.
  const int content_height =
      std::max(shortcut_height, description_height + 2 * description_delta_y);
  calculated_size_ = gfx::Size(width, content_height + insets.height());
}

}  // namespace keyboard_shortcut_viewer
