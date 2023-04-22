// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/keyboard_shortcut_item_view.h"

#include <memory>
#include <vector>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/public/cpp/keyboard_shortcut_item.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ash/shortcut_viewer/views/bubble_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/styled_label.h"

namespace keyboard_shortcut_viewer {

namespace {

// Light mode color:
constexpr int kIconSize = 16;

constexpr SkColor kShortcutBubbleSeparatorColorLight =
    SkColorSetARGB(0xFF, 0x1A, 0x73, 0xE8);

// Custom separator view to enable updating OnThemeChanged.
class KSVSeparatorImageView : public views::ImageView {
 public:
  KSVSeparatorImageView() {
    color_provider_ = ash::ColorProvider::Get();
    ConfigureImage();
  }

  KSVSeparatorImageView(const KSVSeparatorImageView&) = delete;
  KSVSeparatorImageView operator=(const KSVSeparatorImageView&) = delete;

  ~KSVSeparatorImageView() override = default;

  // views::View:
  void OnThemeChanged() override {
    ConfigureImage();

    views::ImageView::OnThemeChanged();
  }

 private:
  // Configure separator image view depending on color mode.
  void ConfigureImage() {
    DCHECK(color_provider_);
    SkColor kShortcutBubbleSeparatorColor = kShortcutBubbleSeparatorColorLight;
    if (ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled()) {
      kShortcutBubbleSeparatorColor = color_provider_->GetContentLayerColor(
          ash::ColorProvider::ContentLayerType::kTextColorSecondary);
    }
    SetImage(gfx::CreateVectorIcon(ash::kKsvSeparatorPlusIcon,
                                   kShortcutBubbleSeparatorColor));
    SetImageSize(gfx::Size(kIconSize, kIconSize));
  }

  raw_ptr<ash::ColorProvider, ExperimentalAsh> color_provider_;  // Not owned.
};

// Creates the separator view between bubble views of modifiers and key.
std::unique_ptr<views::View> CreateSeparatorView() {
  std::unique_ptr<KSVSeparatorImageView> separator_view =
      std::make_unique<KSVSeparatorImageView>();
  return separator_view;
}

// Creates the bubble view for modifiers and key.
std::unique_ptr<views::View> CreateBubbleView(const std::u16string& bubble_text,
                                              ui::KeyboardCode key_code) {
  auto bubble_view = std::make_unique<BubbleView>();
  const gfx::VectorIcon* vector_icon =
      ash::GetVectorIconForKeyboardCode(key_code);
  if (vector_icon)
    bubble_view->SetIcon(*vector_icon);
  else
    bubble_view->SetText(bubble_text);
  return bubble_view;
}

}  // namespace

KeyboardShortcutItemView::KeyboardShortcutItemView(
    const ash::KeyboardShortcutItem& item,
    ash::ShortcutCategory category)
    : shortcut_item_(&item), category_(category) {
  description_label_view_ =
      AddChildView(std::make_unique<views::StyledLabel>());
  description_label_view_->SetText(
      l10n_util::GetStringUTF16(item.description_message_id));
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |description_label_view_| always
  // align to left.
  description_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);

  std::vector<size_t> offsets;
  std::vector<std::u16string> replacement_strings;
  std::vector<std::u16string> accessible_names;
  const size_t shortcut_key_codes_size = item.shortcut_key_codes.size();
  offsets.reserve(shortcut_key_codes_size);
  replacement_strings.reserve(shortcut_key_codes_size);
  accessible_names.reserve(shortcut_key_codes_size);
  bool has_invalid_dom_key = false;
  for (ui::KeyboardCode key_code : item.shortcut_key_codes) {
    auto iter = GetKeycodeToString16Cache()->find(key_code);
    if (iter == GetKeycodeToString16Cache()->end()) {
      iter = GetKeycodeToString16Cache()
                 ->emplace(key_code, ash::GetStringForKeyboardCode(key_code))
                 .first;
    }

    // Get the string for the |DomKey|.
    std::u16string dom_key_string = iter->second;

    // There are two existing browser shortcuts which should not be
    // positionally remapped, these are manually overridden here. When
    // this app is deprecated, the new shortcut app will source the shortcut
    // data directly from ash or the browser and will not remap the browser
    // set. Since they are duplicated and intermixed in this app they need
    // to be explicitly omitted.
    const bool dont_remap_position =
        item.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_PLUS ||
        item.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_MINUS;
    if (dont_remap_position) {
      dom_key_string = ash::GetStringForKeyboardCode(
          key_code, /*remap_positional_key=*/false);
    }

    // If the |key_code| has no mapped |dom_key_string|, we use alternative
    // string to indicate that the shortcut is not supported by current keyboard
    // layout.
    if (dom_key_string.empty()) {
      replacement_strings.clear();
      accessible_names.clear();
      has_invalid_dom_key = true;
      break;
    }

    std::u16string accessible_name = GetAccessibleNameForKeyboardCode(key_code);
    accessible_names.push_back(accessible_name.empty() ? dom_key_string
                                                       : accessible_name);
    replacement_strings.push_back(std::move(dom_key_string));
  }

  int shortcut_message_id;
  if (has_invalid_dom_key) {
    // |shortcut_message_id| should never be used if the shortcut is not
    // supported on the current keyboard layout.
    shortcut_message_id = -1;
  } else if (item.shortcut_message_id) {
    shortcut_message_id = *item.shortcut_message_id;
  } else {
    // Automatically determine the shortcut message based on the number of
    // replacement strings.
    // As there are separators inserted between the modifiers, a shortcut with
    // N modifiers has 2*N + 1 replacement strings.
    switch (replacement_strings.size()) {
      case 1:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_KEY;
        break;
      case 3:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_MODIFIER_ONE_KEY;
        break;
      case 5:
        shortcut_message_id = IDS_KSV_SHORTCUT_TWO_MODIFIERS_ONE_KEY;
        break;
      case 7:
        shortcut_message_id = IDS_KSV_SHORTCUT_THREE_MODIFIERS_ONE_KEY;
        break;
      default:
        NOTREACHED() << "Automatically determined shortcut has "
                     << replacement_strings.size() << " replacement strings.";
    }
  }

  std::u16string shortcut_string;
  std::u16string accessible_string;
  if (replacement_strings.empty()) {
    shortcut_string = l10n_util::GetStringUTF16(
        has_invalid_dom_key ? IDS_KSV_KEY_NO_MAPPING : shortcut_message_id);
    accessible_string = shortcut_string;
  } else {
    shortcut_string = l10n_util::GetStringFUTF16(shortcut_message_id,
                                                 replacement_strings, &offsets);
    accessible_string = l10n_util::GetStringFUTF16(
        shortcut_message_id, accessible_names, /*offsets=*/nullptr);
  }
  shortcut_label_view_ = AddChildView(std::make_unique<views::StyledLabel>());
  shortcut_label_view_->SetText(shortcut_string);
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |shortcut_label_view_| always
  // align to right.
  shortcut_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_LEFT : gfx::ALIGN_RIGHT);
  DCHECK_EQ(replacement_strings.size(), offsets.size());
  // TODO(wutao): make this reliable.
  // If the replacement string is "+ ", it indicates to insert a separator view.
  const std::u16string separator_string = u"+ ";
  for (size_t i = 0; i < offsets.size(); ++i) {
    views::StyledLabel::RangeStyleInfo style_info;
    const std::u16string& replacement_string = replacement_strings[i];
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

  constexpr int kVerticalPadding = 10;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalPadding, 0, kVerticalPadding, 0)));

  SetAccessibilityProperties(
      ax::mojom::Role::kListItem,
      l10n_util::GetStringFUTF16(IDS_CONCAT_TWO_STRINGS_WITH_COMMA,
                                 description_label_view_->GetText(),
                                 accessible_string));

  // Use leaf list item role so that name is spoken by screen reader, but
  // redundant child label text is not also spoken.
  GetViewAccessibility().OverrideIsLeaf(true);
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
std::map<ui::KeyboardCode, std::u16string>*
KeyboardShortcutItemView::GetKeycodeToString16Cache() {
  static base::NoDestructor<std::map<ui::KeyboardCode, std::u16string>>
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

BEGIN_METADATA(KeyboardShortcutItemView, views::View)
END_METADATA

}  // namespace keyboard_shortcut_viewer
