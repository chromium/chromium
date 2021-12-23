// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_targeter_delegate.h"

namespace ash {
namespace {

// The padding values of the DesksTemplatesItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// The preferred size of the whole DesksTemplatesItemView.
constexpr gfx::Size kPreferredSize(220, 120);

// The corner radius for the DesksTemplatesItemView.
constexpr int kCornerRadius = 16;

// The margin for the delete button.
constexpr int kDeleteButtonMargin = 8;

// The minimum template name view width.
constexpr int kMinTemplateNameViewWidth = 56;

// The margin between the grid item contents and the card container.
constexpr int kGridItemMargin = 24;
constexpr int kTimeViewHeight = 20;

constexpr char kAmPmTimeDateFmtStr[] = "%d:%02d%s, %d-%02d-%02d";

// TODO(richui): This is a placeholder text format. Update this once specs are
// done.
std::u16string GetTimeStr(base::Time timestamp) {
  base::Time::Exploded exploded_time;
  timestamp.LocalExplode(&exploded_time);

  const int noon = 12;
  int hour = exploded_time.hour % noon;
  if (hour == 0)
    hour += noon;

  std::string time = base::StringPrintf(
      kAmPmTimeDateFmtStr, hour, exploded_time.minute,
      (exploded_time.hour >= noon ? "pm" : "am"), exploded_time.year,
      exploded_time.month, exploded_time.day_of_month);
  return base::UTF8ToUTF16(time);
}

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView(DeskTemplate* desk_template)
    : desk_template_(desk_template) {
  auto launch_template_callback = base::BindRepeating(
      &DesksTemplatesItemView::OnGridItemPressed, base::Unretained(this));

  const std::u16string template_name = desk_template_->template_name();

  views::View* spacer;
  views::BoxLayoutView* card_container;
  views::Builder<DesksTemplatesItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(template_name)
      .SetCallback(std::move(launch_template_callback))
      .SetBackground(views::CreateRoundedRectBackground(
          AshColorProvider::Get()->GetControlsLayerColor(
              AshColorProvider::ControlsLayerType::
                  kControlBackgroundColorInactive),
          kCornerRadius))
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&card_container)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetInsideBorderInsets(
                  gfx::Insets(kVerticalPaddingDp, kHorizontalPaddingDp))
              .AddChildren(
                  views::Builder<DesksTemplatesNameView>()
                      .CopyAddressTo(&name_view_)
                      .SetText(template_name)
                      .SetAccessibleName(template_name),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetText(GetTimeStr(desk_template_->created_time()))
                      .SetPreferredSize(gfx::Size(
                          kPreferredSize.width() - kGridItemMargin * 2,
                          kTimeViewHeight)),
                  views::Builder<views::View>().CopyAddressTo(&spacer),
                  views::Builder<DesksTemplatesIconContainer>().CopyAddressTo(
                      &icon_container_view_)),
          views::Builder<views::View>().CopyAddressTo(&hover_container_))
      .BuildChildren();

  // TODO(crbug.com/1267470): Make `PillButton` work with views::Builder.
  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnGridItemPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON),
      PillButton::Type::kIconless, /*icon=*/nullptr));

  delete_button_ = hover_container_->AddChildView(std::make_unique<CloseButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnDeleteButtonPressed,
                          base::Unretained(this)),
      CloseButton::Type::kMedium));

  name_view_->SetTextAndElideIfNeeded(template_name);
  name_view_->set_controller(this);
  name_view_observation_.Observe(name_view_);

  hover_container_->SetUseDefaultFillLayout(true);
  hover_container_->SetVisible(false);

  icon_container_view_->PopulateIconContainerFromTemplate(desk_template_);
  icon_container_view_->SetVisible(true);
  card_container->SetFlexForView(spacer, 1);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<DesksTemplatesItemView*>(view)->IsViewHighlighted();
  });

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

DesksTemplatesItemView::~DesksTemplatesItemView() {
  name_view_observation_.Reset();
}

void DesksTemplatesItemView::UpdateHoverButtonsVisibility(
    const gfx::Point& screen_location,
    bool is_touch) {
  gfx::Point location_in_view = screen_location;
  ConvertPointFromScreen(this, &location_in_view);

  // For switch access, setting the hover buttons to visible allows users to
  // navigate to it.
  const bool visible =
      !is_template_name_being_modified_ &&
      ((is_touch && HitTestPoint(location_in_view)) ||
       (!is_touch && IsMouseHovered()) ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());
  hover_container_->SetVisible(visible);
  icon_container_view_->SetVisible(!visible);
}

bool DesksTemplatesItemView::IsTemplateNameBeingModified() const {
  return name_view_->HasFocus();
}

void DesksTemplatesItemView::Layout() {
  views::View::Layout();

  LayoutTemplateNameView();

  const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
  DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
  delete_button_->SetBoundsRect(
      gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                kDeleteButtonMargin, delete_button_size.width(),
                delete_button_size.height()));

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize();
  launch_button_->SetBoundsRect(gfx::Rect(
      {(width() - launch_button_preferred_size.width()) / 2,
       height() - launch_button_preferred_size.height() - kVerticalPaddingDp},
      launch_button_preferred_size));
}

void DesksTemplatesItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  const SkColor control_background_color_inactive =
      color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);

  GetBackground()->SetNativeControlColor(control_background_color_inactive);

  time_view_->SetBackgroundColor(control_background_color_inactive);
  time_view_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));

  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

void DesksTemplatesItemView::OnViewFocused(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about focus on
  // `name_view_`.
  if (observed_view == this)
    return;

  DCHECK_EQ(observed_view, name_view_);
  is_template_name_being_modified_ = true;

  // Assume we should commit the name change unless `HandleKeyEvent` detects the
  // user pressed the escape key.
  should_commit_name_changes_ = true;
  name_view_->UpdateViewAppearance();

  // Hide the hover container when we are modifying the template name.
  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);

  // Set the unelided template name so that the full name shows up for the user
  // to be able to change it.
  name_view_->SetText(desk_template_->template_name());

  // Set the Overview highlight to move focus with the `name_view_`.
  auto* highlight_controller = Shell::Get()
                                   ->overview_controller()
                                   ->overview_session()
                                   ->highlight_controller();
  if (highlight_controller->IsFocusHighlightVisible())
    highlight_controller->MoveHighlightToView(name_view_);

  if (!defer_select_all_)
    name_view_->SelectAll(false);
}

void DesksTemplatesItemView::OnViewBlurred(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about blur on
  // `name_view_`.
  if (observed_view == this)
    return;

  // If we exit overview while the `name_view_` is still focused, the shutdown
  // sequence will reset the presenter before `OnViewBlurred` gets called. This
  // checks and makes sure that we don't call the presenter while trying to
  // shutdown the overview session.
  // `overview_session` may also be null as `OnViewBlurred` may be called after
  // the owning widget is no longer owned by the session for overview exit
  // animation. See https://crbug.com/1281422.
  // TODO(richui): Revisit this once the behavior of the template name when
  // exiting overview is determined.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  if (!overview_session || overview_session->is_shutting_down())
    return;

  DCHECK_EQ(observed_view, name_view_);
  is_template_name_being_modified_ = false;
  defer_select_all_ = false;
  name_view_->UpdateViewAppearance();

  // Collapse the whitespace for the text first before comparing it or trying to
  // commit the name in order to prevent duplicate name issues.
  name_view_->SetText(
      base::CollapseWhitespace(name_view_->GetText(),
                               /*trim_sequences_with_line_breaks=*/false));

  // When committing the name, do not allow an empty template name. Also, don't
  // commit the name changes if the view was blurred from the user pressing the
  // escape key (identified by `should_commit_name_changes_`). Revert back to
  // the original name.
  if (!should_commit_name_changes_ || name_view_->GetText().empty() ||
      desk_template_->template_name() == name_view_->GetText()) {
    OnTemplateNameChanged(desk_template_->template_name());
    return;
  }

  auto updated_template = desk_template_->Clone();
  updated_template->set_template_name(name_view_->GetText());
  OnTemplateNameChanged(updated_template->template_name());

  DesksTemplatesPresenter::Get()->SaveOrUpdateDeskTemplate(
      /*is_update=*/false, std::move(updated_template));
}

views::Button::KeyClickAction DesksTemplatesItemView::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  // Prevents any key events from activating a button click while the template
  // name is being modified.
  if (is_template_name_being_modified_)
    return KeyClickAction::kNone;

  return Button::GetKeyClickActionForEvent(event);
}

void DesksTemplatesItemView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, name_view_);
  DCHECK(is_template_name_being_modified_);

  // To avoid potential security and memory issues, we don't allow template
  // names to have an unbounded length. Therefore we trim if needed at
  // `kMaxLength` UTF-16 boundary. Note that we don't care about code point
  // boundaries in this case.
  if (new_contents.size() > DesksTextfield::kMaxLength) {
    std::u16string trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DesksTextfield::kMaxLength);
    name_view_->SetText(trimmed_new_contents);
  }

  Layout();
}

bool DesksTemplatesItemView::HandleKeyEvent(views::Textfield* sender,
                                            const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, name_view_);
  DCHECK(is_template_name_being_modified_);

  // Pressing enter or escape should blur the focus away from `name_view_` so
  // that editing the template's name ends. Pressing tab should do the same, but
  // is handled in `OverviewSession`.
  if (key_event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }

  // If the escape key was pressed, `should_commit_name_changes_` is set to
  // false so that `OnViewBlurred` knows that it should not change the name of
  // the template.
  if (key_event.key_code() == ui::VKEY_ESCAPE)
    should_commit_name_changes_ = false;

  DesksTemplatesNameView::CommitChanges(GetWidget());

  return true;
}

bool DesksTemplatesItemView::HandleMouseEvent(
    views::Textfield* sender,
    const ui::MouseEvent& mouse_event) {
  DCHECK_EQ(sender, name_view_);

  switch (mouse_event.type()) {
    case ui::ET_MOUSE_PRESSED:
      // If this is the first mouse press on the `name_view_`, then it's not
      // focused yet. `OnViewFocused()` should not select all text, since it
      // will be undone by the mouse release event. Instead we defer it until we
      // get the mouse release event.
      if (!is_template_name_being_modified_)
        defer_select_all_ = true;
      break;

    case ui::ET_MOUSE_RELEASED:
      if (defer_select_all_) {
        defer_select_all_ = false;
        // The user may have already clicked and dragged to select some range
        // other than all the text. In this case, don't mess with an existing
        // selection.
        if (!name_view_->HasSelection()) {
          name_view_->SelectAll(false);
        }
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

views::View* DesksTemplatesItemView::TargetForRect(views::View* root,
                                                   const gfx::Rect& rect) {
  // With the design of the template card having the textfield within a
  // clickable button, as well as having the grid view be a `PreTargetHandler`,
  // we needed to make `this` a `ViewTargeterDelegate` for the view event
  // targeter in order to allow the `name_view_` to be specifically targeted and
  // focused.
  if (root == this && name_view_->GetMirroredBounds().Contains(rect))
    return name_view_;
  return views::ViewTargeterDelegate::TargetForRect(root, rect);
}

void DesksTemplatesItemView::OnDeleteTemplate() {
  // Notify the highlight controller that we're going away.
  OverviewHighlightController* highlight_controller =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->highlight_controller();
  DCHECK(highlight_controller);
  highlight_controller->OnViewDestroyingOrDisabling(this);
  highlight_controller->OnViewDestroyingOrDisabling(name_view_);

  DesksTemplatesPresenter::Get()->DeleteEntry(
      desk_template_->uuid().AsLowercaseString());
}

void DesksTemplatesItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->ShowDeleteDialog(
      Shell::GetPrimaryRootWindow(), name_view_->GetAccessibleName(),
      base::BindOnce(&DesksTemplatesItemView::OnDeleteTemplate,
                     base::Unretained(this)));
}

void DesksTemplatesItemView::OnGridItemPressed() {
  if (is_template_name_being_modified_) {
    DesksTemplatesNameView::CommitChanges(GetWidget());
    return;
  }

  DesksTemplatesPresenter::Get()->LaunchDeskTemplate(
      desk_template_->uuid().AsLowercaseString());
}

void DesksTemplatesItemView::OnTemplateNameChanged(
    const std::u16string& new_name) {
  if (is_template_name_being_modified_)
    return;

  name_view_->SetTextAndElideIfNeeded(new_name);
  name_view_->SetAccessibleName(new_name);
  SetAccessibleName(new_name);

  Layout();
}

void DesksTemplatesItemView::LayoutTemplateNameView() {
  const int previous_width = name_view_->width();
  const gfx::Size name_view_size = name_view_->GetPreferredSize();
  // The item view's width is supposed to be larger than
  // `kMinTemplateNameViewWidth`, but it might be not the truth for tests with
  // extreme abnormal size of display.
  const int min_width =
      std::min(kPreferredSize.width(), kMinTemplateNameViewWidth);
  // TODO(crbug.com/1264174): Investigate the best way to get this to work with
  // the enterprise indicator. Possibly wrap both in a `BoxLayoutView`.
  const int max_width =
      std::max(kPreferredSize.width() - (kHorizontalPaddingDp * 2),
               kMinTemplateNameViewWidth);
  const int text_width =
      base::clamp(name_view_size.width(), min_width, max_width);
  gfx::Rect name_view_bounds{name_view_->bounds()};
  name_view_bounds.set_width(text_width);

  name_view_->SetBoundsRect(name_view_bounds);

  // A change in the `name_view_`'s width might mean the need to elide the text
  // differently.
  if (previous_width != name_view_bounds.width())
    OnTemplateNameChanged(desk_template_->template_name());
}

views::View* DesksTemplatesItemView::GetView() {
  return this;
}

void DesksTemplatesItemView::MaybeActivateHighlightedView() {
  OnGridItemPressed();
}

void DesksTemplatesItemView::MaybeCloseHighlightedView() {
  OnDeleteButtonPressed();
}

void DesksTemplatesItemView::MaybeSwapHighlightedView(bool right) {}

void DesksTemplatesItemView::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void DesksTemplatesItemView::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(DesksTemplatesItemView, views::Button)
END_METADATA

}  // namespace ash
