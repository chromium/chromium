// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/user_card_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/media_controller.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/user/rounded_image_view.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositing_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

namespace {

const int kUserDetailsVerticalPadding = 5;

// The invisible word joiner character, used as a marker to indicate the start
// and end of the user's display name in the public account user card's text.
const base::char16 kDisplayNameMark[] = {0x2060, 0};

views::View* CreateUserAvatarView(int user_index) {
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index);

  RoundedImageView* image_view = new RoundedImageView(kTrayItemSize / 2);
  if (user_session->user_info->type == user_manager::USER_TYPE_GUEST) {
    gfx::ImageSkia icon =
        gfx::CreateVectorIcon(kSystemMenuGuestIcon, kMenuIconColor);
    image_view->SetImage(icon, icon.size());
  } else {
    image_view->SetImage(user_session->user_info->avatar->image,
                         gfx::Size(kTrayItemSize, kTrayItemSize));
  }

  image_view->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      (kTrayPopupItemMinStartWidth - image_view->GetPreferredSize().width()) /
      2)));
  return image_view;
}

// The user details shown in public account mode. This is essentially a label
// but with custom painting code as the text is styled with multiple colors and
// contains a link.
class PublicAccountUserDetails : public views::View,
                                 public views::LinkListener {
 public:
  PublicAccountUserDetails();
  ~PublicAccountUserDetails() override;

 private:
  // Overridden from views::View.
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from views::LinkListener.
  void LinkClicked(views::Link* source, int event_flags) override;

  // Calculate a preferred size that ensures the label text and the following
  // link do not wrap over more than three lines in total for aesthetic reasons
  // if possible.
  void DeterminePreferredSize();

  base::string16 text_;
  views::Link* learn_more_;
  std::vector<std::unique_ptr<gfx::RenderText>> lines_;

  DISALLOW_COPY_AND_ASSIGN(PublicAccountUserDetails);
};

PublicAccountUserDetails::PublicAccountUserDetails() : learn_more_(nullptr) {
  const int inner_padding =
      kTrayPopupPaddingHorizontal - kTrayPopupPaddingBetweenItems;
  const bool rtl = base::i18n::IsRTL();
  SetBorder(views::CreateEmptyBorder(
      kUserDetailsVerticalPadding, rtl ? 0 : inner_padding,
      kUserDetailsVerticalPadding, rtl ? inner_padding : 0));

  // Retrieve the user's display name and wrap it with markers.
  // Note that since this is a public account it always has to be the primary
  // user.
  base::string16 display_name =
      base::UTF8ToUTF16(Shell::Get()
                            ->session_controller()
                            ->GetUserSession(0)
                            ->user_info->display_name);
  base::RemoveChars(display_name, kDisplayNameMark, &display_name);
  display_name = kDisplayNameMark[0] + display_name + kDisplayNameMark[0];
  // Retrieve the domain managing the device and wrap it with markers.
  base::string16 domain = base::UTF8ToUTF16(Shell::Get()
                                                ->system_tray_model()
                                                ->enterprise_domain()
                                                ->enterprise_display_domain());
  base::RemoveChars(domain, kDisplayNameMark, &domain);
  base::i18n::WrapStringWithLTRFormatting(&domain);
  // Retrieve the label text, inserting the display name and domain.
  text_ = l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_PUBLIC_LABEL,
                                     display_name, domain);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE));
  learn_more_->SetUnderline(false);
  learn_more_->set_listener(this);
  AddChildView(learn_more_);

  DeterminePreferredSize();
}

PublicAccountUserDetails::~PublicAccountUserDetails() = default;

void PublicAccountUserDetails::Layout() {
  lines_.clear();
  const gfx::Rect contents_area = GetContentsBounds();
  if (contents_area.IsEmpty())
    return;

  // Word-wrap the label text.
  const gfx::FontList font_list;
  std::vector<base::string16> lines;
  gfx::ElideRectangleText(text_, font_list, contents_area.width(),
                          contents_area.height(), gfx::ELIDE_LONG_WORDS,
                          &lines);
  // Loop through the lines, creating a renderer for each.
  gfx::Point position = contents_area.origin();
  gfx::Range display_name(gfx::Range::InvalidRange());
  for (auto it = lines.begin(); it != lines.end(); ++it) {
    auto line = gfx::RenderText::CreateHarfBuzzInstance();
    line->SetDirectionalityMode(gfx::DIRECTIONALITY_FROM_UI);
    line->SetText(*it);
    const gfx::Size size(contents_area.width(), line->GetStringSize().height());
    line->SetDisplayRect(gfx::Rect(position, size));
    position.set_y(position.y() + size.height());

    // Set the default text color for the line.
    line->SetColor(kPublicAccountUserCardTextColor);

    // If a range of the line contains the user's display name, apply a custom
    // text color to it.
    if (display_name.is_empty())
      display_name.set_start(it->find(kDisplayNameMark));
    if (!display_name.is_empty()) {
      display_name.set_end(
          it->find(kDisplayNameMark, display_name.start() + 1));
      gfx::Range line_range(0, it->size());
      line->ApplyColor(kPublicAccountUserCardNameColor,
                       display_name.Intersect(line_range));
      // Update the range for the next line.
      if (display_name.end() >= line_range.end())
        display_name.set_start(0);
      else
        display_name = gfx::Range::InvalidRange();
    }

    lines_.push_back(std::move(line));
  }

  // Position the link after the label text, separated by a space. If it does
  // not fit onto the last line of the text, wrap the link onto its own line.
  const gfx::Size last_line_size = lines_.back()->GetStringSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  if (contents_area.width() - last_line_size.width() >=
      space_width + link_size.width()) {
    position.set_x(position.x() + last_line_size.width() + space_width);
    position.set_y(position.y() - last_line_size.height());
  }
  position.set_y(position.y() - learn_more_->GetInsets().top());
  gfx::Rect learn_more_bounds(position, link_size);
  learn_more_bounds.Intersect(contents_area);
  if (base::i18n::IsRTL()) {
    const gfx::Insets insets = GetInsets();
    learn_more_bounds.Offset(insets.right() - insets.left(), 0);
  }
  learn_more_->SetBoundsRect(learn_more_bounds);
}

void PublicAccountUserDetails::OnPaint(gfx::Canvas* canvas) {
  for (const auto& line : lines_)
    line->Draw(canvas);

  views::View::OnPaint(canvas);
}

void PublicAccountUserDetails::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kStaticText;
  node_data->SetName(text_);
}

void PublicAccountUserDetails::LinkClicked(views::Link* source,
                                           int event_flags) {
  DCHECK_EQ(source, learn_more_);
  Shell::Get()->system_tray_model()->client_ptr()->ShowPublicAccountInfo();
}

void PublicAccountUserDetails::DeterminePreferredSize() {
  const gfx::FontList font_list;
  const gfx::Size link_size = learn_more_->GetPreferredSize();
  const int space_width =
      gfx::GetStringWidth(base::ASCIIToUTF16(" "), font_list);
  const gfx::Insets insets = GetInsets();
  int min_width = link_size.width();
  int max_width =
      gfx::GetStringWidth(text_, font_list) + space_width + link_size.width();

  // Do a binary search for the minimum width that ensures no more than three
  // lines are needed. The lower bound is the minimum of the current bubble
  // width and the width of the link (as no wrapping is permitted inside the
  // link). The upper bound is the maximum of the largest allowed bubble width
  // and the sum of the label text and link widths when put on a single line.
  std::vector<base::string16> lines;
  while (min_width < max_width) {
    lines.clear();
    const int width = (min_width + max_width) / 2;
    const bool too_narrow =
        gfx::ElideRectangleText(text_, font_list, width, INT_MAX,
                                gfx::TRUNCATE_LONG_WORDS, &lines) != 0;
    int line_count = lines.size();
    if (!too_narrow && line_count == 3 &&
        width - gfx::GetStringWidth(lines.back(), font_list) <=
            space_width + link_size.width())
      ++line_count;
    if (too_narrow || line_count > 3)
      min_width = width + 1;
    else
      max_width = width;
  }

  // Calculate the corresponding height and set the preferred size.
  lines.clear();
  gfx::ElideRectangleText(text_, font_list, min_width, INT_MAX,
                          gfx::TRUNCATE_LONG_WORDS, &lines);
  int line_count = lines.size();
  if (min_width - gfx::GetStringWidth(lines.back(), font_list) <=
      space_width + link_size.width()) {
    ++line_count;
  }
  const int line_height = font_list.GetHeight();
  const int link_extra_height = std::max(
      link_size.height() - learn_more_->GetInsets().top() - line_height, 0);
  SetPreferredSize(gfx::Size(
      min_width + insets.width(),
      line_count * line_height + link_extra_height + insets.height()));
}

}  // namespace

UserCardView::UserCardView(int user_index) : user_index_(user_index) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      kTrayPopupLabelHorizontalPadding));
  layout->set_minimum_cross_axis_size(kTrayPopupItemMinHeight);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);

  Shell::Get()->media_controller()->AddObserver(this);

  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index_);
  if (user_session->user_info->type == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    AddPublicModeUserContent();
  else
    AddUserContent(layout);
}

UserCardView::~UserCardView() {
  Shell::Get()->media_controller()->RemoveObserver(this);
}

void UserCardView::SetSuppressCaptureIcon(bool suppressed) {
  DCHECK_EQ(0, user_index_);
  media_capture_container_->SetVisible(!suppressed);
}

void UserCardView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kStaticText;
  std::vector<base::string16> labels;

  // Construct the name by concatenating descendants' names.
  std::list<views::View*> descendants;
  descendants.push_back(this);
  while (!descendants.empty()) {
    auto* view = descendants.front();
    descendants.pop_front();
    if (view != this) {
      ui::AXNodeData descendant_data;
      view->GetAccessibleNodeData(&descendant_data);
      base::string16 label = descendant_data.GetString16Attribute(
          ax::mojom::StringAttribute::kName);
      // If we find a non-empty name, use that and don't descend further into
      // the tree.
      if (!label.empty()) {
        labels.push_back(label);
        continue;
      }
    }

    // This view didn't have its own name, so look over its children.
    for (int i = view->child_count() - 1; i >= 0; --i)
      descendants.push_front(view->child_at(i));
  }
  node_data->SetName(base::JoinString(labels, base::ASCIIToUTF16(" ")));
}

void UserCardView::OnMediaCaptureChanged(
    const base::flat_map<AccountId, mojom::MediaCaptureState>& capture_states) {
  mojom::MediaCaptureState state = mojom::MediaCaptureState::NONE;
  auto account_id = Shell::Get()
                        ->session_controller()
                        ->GetUserSession(user_index_)
                        ->user_info->account_id;
  if (is_active_user()) {
    int cumulative_state = 0;
    // The active user reports media capture states for all /other/ users.
    for (const auto& entry : capture_states) {
      if (entry.first == account_id)
        continue;
      cumulative_state |= static_cast<int32_t>(entry.second);
    }
    state = static_cast<mojom::MediaCaptureState>(cumulative_state);
  } else {
    auto matched = capture_states.find(account_id);
    state = matched->second;
  }

  int res_id = 0;
  switch (state) {
    case mojom::MediaCaptureState::AUDIO_VIDEO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO_VIDEO;
      break;
    case mojom::MediaCaptureState::AUDIO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_AUDIO;
      break;
    case mojom::MediaCaptureState::VIDEO:
      res_id = IDS_ASH_STATUS_TRAY_MEDIA_RECORDING_VIDEO;
      break;
    case mojom::MediaCaptureState::NONE:
      break;
  }
  if (res_id)
    media_capture_icon_->set_tooltip_text(l10n_util::GetStringUTF16(res_id));
  media_capture_icon_->SetVisible(!!res_id);
  Layout();
}

void UserCardView::AddPublicModeUserContent() {
  // Public account user should be the only user in the system.
  DCHECK_EQ(0, user_index_);
  views::View* avatar = CreateUserAvatarView(user_index_);
  AddChildView(avatar);
  AddChildView(new PublicAccountUserDetails());
}

void UserCardView::AddUserContent(views::BoxLayout* layout) {
  AddChildView(CreateUserAvatarView(user_index_));
  const mojom::UserSession* const user_session =
      Shell::Get()->session_controller()->GetUserSession(user_index_);
  const bool is_guest =
      user_session->user_info->type == user_manager::USER_TYPE_GUEST;
  const base::string16 user_name_string =
      is_guest ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL)
               : base::UTF8ToUTF16(user_session->user_info->display_name);
  user_name_ = new views::Label(user_name_string);
  user_name_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  TrayPopupItemStyle user_name_style(
      TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
  user_name_style.SetupLabel(user_name_);

  TrayPopupItemStyle user_email_style(TrayPopupItemStyle::FontStyle::CAPTION);
  user_email_style.set_color_style(TrayPopupItemStyle::ColorStyle::INACTIVE);
  auto* user_email = new views::Label();
  base::string16 user_email_string;
  if (!is_guest) {
    user_email_string =
        Shell::Get()->session_controller()->IsUserLegacySupervised()
            ? l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL)
            : base::UTF8ToUTF16(user_session->user_info->display_email);
  }
  user_email->SetText(user_email_string);
  user_email->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  user_email_style.SetupLabel(user_email);
  user_email->SetVisible(!user_email_string.empty());
  user_email->set_collapse_when_hidden(true);

  views::View* stack_of_labels = new views::View;
  AddChildView(stack_of_labels);
  layout->SetFlexForView(stack_of_labels, 1);
  stack_of_labels->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  stack_of_labels->AddChildView(user_name_);
  stack_of_labels->AddChildView(user_email);
  // The name and email have different font sizes. This border is designed
  // to make both views take up equal space so the whitespace between them
  // is centered on the vertical midpoint.
  int user_email_bottom_pad = user_name_->GetPreferredSize().height() -
                              user_email->GetPreferredSize().height();
  user_email->SetBorder(
      views::CreateEmptyBorder(0, 0, user_email_bottom_pad, 0));

  media_capture_icon_ = new views::ImageView;
  media_capture_icon_->SetImage(
      gfx::CreateVectorIcon(kSystemTrayRecordingIcon, gfx::kGoogleRed700));
  const int media_capture_width = kTrayPopupItemMinEndWidth;
  media_capture_icon_->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      0,
      (media_capture_width - media_capture_icon_->GetPreferredSize().width()) /
          2)));
  media_capture_icon_->set_id(VIEW_ID_USER_VIEW_MEDIA_INDICATOR);
  media_capture_icon_->SetVisible(false);

  media_capture_container_ = new views::View();
  media_capture_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  media_capture_container_->AddChildView(media_capture_icon_);
  AddChildView(media_capture_container_);

  Shell::Get()->media_controller()->RequestCaptureState();
}

}  // namespace tray
}  // namespace ash
