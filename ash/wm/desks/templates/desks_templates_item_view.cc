// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_grid_view.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// The padding values of the DesksTemplatesItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// Shift the name view by 2dp so that the text is aligned with the date view.
// The name view itself has a background, which shows on hover or focus, is not
// aligned with the date view.
constexpr gfx::Insets kNameExtraInsets(0, -2, 0, 0);

// The preferred size of the whole DesksTemplatesItemView.
constexpr gfx::Size kPreferredSize(220, 120);

// The corner radius for the DesksTemplatesItemView.
constexpr int kCornerRadius = 16;

// The margin for the delete button.
constexpr int kDeleteButtonMargin = 8;

// The distance from the bottom of the launch button to the bottom of `this`.
constexpr int kLaunchButtonDistanceFromBottomDp = 14;

// The preferred width of the container that houses the template name textfield
// and managed status indicator and the time label.
constexpr int kTemplateNameAndTimePreferredWidth =
    kPreferredSize.width() - kHorizontalPaddingDp * 2;

// The height of the view which contains the time of the template.
constexpr int kTimeViewHeight = 24;

// The spacing between the textfield and the managed status icon.
constexpr int kManagedStatusIndicatorSpacing = 8;
constexpr int kManagedStatusIndicatorSize = 20;

std::u16string GetTimeStr(base::Time timestamp) {
  std::u16string date, time, time_str;

  // Returns empty if `timestamp` is out of relative date range, which is
  // yesterday and today as of now. Please see `ui/base/l10n/time_format.h` for
  // more details.
  date = ui::TimeFormat::RelativeDate(timestamp, nullptr);
  if (date.empty()) {
    // Syntax `yMMMdjmm` is used by the File App if it's not a relative date.
    // Please note, this might be slightly different for different locales.
    // Examples:
    //  `en-US` - `Jan 1, 2022, 10:30 AM`
    //  `zh-CN` - `2022年1月1日 10:30`
    time_str = base::TimeFormatWithPattern(timestamp, "yMMMdjmm");
  } else {
    // If it's a relative date, just append `jmm` to it.
    // Please note, this might be slightly different for different locales.
    // Examples:
    //  `en-US` - `Today 10:30 AM`
    //  `zh-CN` - `今天 10:30`
    time_str = date + u" " + base::TimeFormatWithPattern(timestamp, "jmm");
  }

  return time_str;
}

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView(
    const DeskTemplate* desk_template)
    : desk_template_(desk_template->Clone()) {
  auto launch_template_callback =
      base::BindRepeating(&DesksTemplatesItemView::OnGridItemPressed,
                          weak_ptr_factory_.GetWeakPtr());

  const std::u16string template_name = desk_template_->template_name();
  auto* color_provider = AshColorProvider::Get();
  const bool is_admin_managed =
      desk_template_->source() == DeskTemplateSource::kPolicy;

  views::Builder<DesksTemplatesItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(template_name)
      .SetCallback(std::move(launch_template_callback))
      .SetBackground(views::CreateRoundedRectBackground(
          color_provider->GetBaseLayerColor(
              AshColorProvider::BaseLayerType::kTransparent80),
          kCornerRadius))
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetInteriorMargin(
                  gfx::Insets(kVerticalPaddingDp, kHorizontalPaddingDp))
              // TODO(richui): Consider splitting some of the children into
              // different files and/or classes.
              .AddChildren(
                  views::Builder<views::BoxLayoutView>()
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetBetweenChildSpacing(kManagedStatusIndicatorSpacing)
                      .SetPreferredSize(gfx::Size(
                          kTemplateNameAndTimePreferredWidth,
                          DesksTemplatesNameView::kTemplateNameViewHeight))
                      .SetProperty(views::kMarginsKey, kNameExtraInsets)
                      .AddChildren(
                          views::Builder<DesksTemplatesNameView>()
                              .CopyAddressTo(&name_view_)
                              .SetController(this)
                              .SetText(template_name)
                              .SetAccessibleName(template_name)
                              .SetReadOnly(!desk_template_->IsModifiable()),
                          views::Builder<views::ImageView>()
                              .SetPreferredSize(
                                  gfx::Size(kManagedStatusIndicatorSize,
                                            kManagedStatusIndicatorSize))
                              .SetImage(gfx::CreateVectorIcon(
                                  chromeos::kEnterpriseIcon,
                                  kManagedStatusIndicatorSize,
                                  color_provider->GetContentLayerColor(
                                      AshColorProvider::ContentLayerType::
                                          kIconColorSecondary)))
                              .SetVisible(is_admin_managed)),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetText(
                          is_admin_managed
                              ? l10n_util::GetStringUTF16(
                                    IDS_ASH_DESKS_TEMPLATES_MANAGEMENT_STATUS_DESCRIPTION)
                              : GetTimeStr(desk_template_->created_time()))
                      .SetPreferredSize(gfx::Size(
                          kTemplateNameAndTimePreferredWidth, kTimeViewHeight)),
                  // View which acts as a spacer, taking up all the available
                  // space between the date and the icons container.
                  views::Builder<views::View>().SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToZero,
                          views::MaximumFlexSizeRule::kUnbounded)),
                  views::Builder<DesksTemplatesIconContainer>()
                      .CopyAddressTo(&icon_container_view_)
                      .PopulateIconContainerFromTemplate(desk_template_.get())
                      .SetVisible(true)),
          views::Builder<views::View>()
              .CopyAddressTo(&hover_container_)
              .SetUseDefaultFillLayout(true)
              .SetVisible(false))
      .BuildChildren();

  // We need to ensure that the layer is non-opaque when animating.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnGridItemPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON),
      PillButton::Type::kIconless, /*icon=*/nullptr));

  delete_button_ = hover_container_->AddChildView(std::make_unique<CloseButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnDeleteButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      CloseButton::Type::kMedium));
  delete_button_->SetVectorIcon(kDeleteIcon);
  delete_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON));
  delete_button_->SetBackgroundColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));

  name_view_observation_.Observe(name_view_);

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

void DesksTemplatesItemView::MaybeRemoveNameNumber() {
  // When there are existing matched Desk name and Template name (ie.
  // "Desk 1"), creating a new template from "Desk 1" will get auto generated
  // template name from backend as "Desk 1 (1)", to prevent template
  // duplication, we show the template view name to be "Desk 1" by removing name
  // number, save template under such name will call out template replace
  // dialog.
  if (FindOtherTemplateWithName(
          DesksController::Get()->active_desk()->name())) {
    // Replace the name number.
    name_view_->SetTemporaryName(DesksController::Get()->active_desk()->name());
    name_view_->SetViewName(DesksController::Get()->active_desk()->name());
  }
}

void DesksTemplatesItemView::ReplaceTemplate(const std::string& uuid) {
  // Make sure we delete the template we are replacing first, so that we don't
  // get template name collisions.
  DesksTemplatesPresenter::Get()->DeleteEntry(uuid);
  UpdateTemplateName();
  RecordReplaceTemplateHistogram();
}

void DesksTemplatesItemView::RevertTemplateName() {
  views::FocusManager* focus_manager = GetFocusManager();
  focus_manager->SetFocusedView(name_view_);
  const auto temporary_name = name_view_->temporary_name();
  name_view_->SetViewName(
      temporary_name.value_or(desk_template_->template_name()));
  name_view_->SelectAll(true);

  name_view_->OnContentsChanged();
}

void DesksTemplatesItemView::UpdateTemplate(
    const DeskTemplate& updated_template) {
  desk_template_ = updated_template.Clone();

  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);

  auto new_name = desk_template_->template_name();
  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  SetAccessibleName(new_name);
  name_view_->OnContentsChanged();

  Layout();
}

void DesksTemplatesItemView::Layout() {
  const int previous_name_view_width = name_view_->width();

  views::View::Layout();

  // A change in the `name_view_`'s width might mean the need to elide the text
  // differently.
  if (previous_name_view_width != name_view_->width())
    OnTemplateNameChanged(desk_template_->template_name());

  const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
  DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
  delete_button_->SetBoundsRect(
      gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                kDeleteButtonMargin, delete_button_size.width(),
                delete_button_size.height()));

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize();
  launch_button_->SetBoundsRect(
      gfx::Rect({(width() - launch_button_preferred_size.width()) / 2,
                 height() - launch_button_preferred_size.height() -
                     kLaunchButtonDistanceFromBottomDp},
                launch_button_preferred_size));
}

void DesksTemplatesItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  GetBackground()->SetNativeControlColor(color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80));

  time_view_->SetBackgroundColor(SK_ColorTRANSPARENT);
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
  const std::u16string user_entered_name =
      base::CollapseWhitespace(name_view_->GetText(),
                               /*trim_sequences_with_line_breaks=*/false);
  name_view_->SetText(user_entered_name);

  // When committing the name, do not allow an empty template name. Also, don't
  // commit the name changes if the view was blurred from the user pressing the
  // escape key (identified by `should_commit_name_changes_`). Revert back to
  // the original name.
  if (!should_commit_name_changes_ || user_entered_name.empty() ||
      desk_template_->template_name() == user_entered_name) {
    OnTemplateNameChanged(desk_template_->template_name());
    return;
  }

  // Check if template name exist, replace existing template if confirmed by
  // user. Use a post task to avoid activating a widget while another widget is
  // still being activated. In this case, we don't want to show the dialog and
  // activate its associated widget until after the desks bar widget is finished
  // activating. See https://crbug.com/1301759.
  auto* template_to_replace = FindOtherTemplateWithName(name_view_->GetText());
  if (template_to_replace) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DesksTemplatesItemView::MaybeShowReplaceDialog,
                       weak_ptr_factory_.GetWeakPtr(), template_to_replace));
    return;
  }

  UpdateTemplateName();
}

void DesksTemplatesItemView::MaybeShowReplaceDialog(
    DesksTemplatesItemView* template_to_replace) {
  // Show replace template dialog. If accepted, replace old template and commit
  // name change.
  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DesksTemplatesDialogController::Get()->ShowReplaceDialog(
      root_window, name_view_->GetText(),
      base::BindOnce(
          &DesksTemplatesItemView::ReplaceTemplate,
          weak_ptr_factory_.GetWeakPtr(),
          template_to_replace->desk_template_->uuid().AsLowercaseString()),
      base::BindOnce(&DesksTemplatesItemView::RevertTemplateName,
                     weak_ptr_factory_.GetWeakPtr()));
}

views::Button::KeyClickAction DesksTemplatesItemView::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  // Prevents any key events from activating a button click while the template
  // name is being modified.
  if (is_template_name_being_modified_)
    return KeyClickAction::kNone;

  return Button::GetKeyClickActionForEvent(event);
}

void DesksTemplatesItemView::UpdateTemplateName() {
  desk_template_->set_template_name(name_view_->GetText());
  OnTemplateNameChanged(desk_template_->template_name());

  DesksTemplatesPresenter::Get()->SaveOrUpdateDeskTemplate(
      /*is_update=*/true, GetWidget()->GetNativeWindow()->GetRootWindow(),
      desk_template_->Clone());
}

void DesksTemplatesItemView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  DCHECK_EQ(sender, name_view_);

  // To avoid potential security and memory issues, we don't allow template
  // names to have an unbounded length. Therefore we trim if needed at
  // `kMaxLength` UTF-16 boundary. Note that we don't care about code point
  // boundaries in this case.
  if (new_contents.size() > DesksTextfield::kMaxLength) {
    std::u16string trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DesksTextfield::kMaxLength);
    name_view_->SetText(trimmed_new_contents);
  }

  name_view_->OnContentsChanged();

  auto* focus_manager = GetWidget()->GetFocusManager();
  if (focus_manager->GetFocusedView() != name_view_) {
    // The text editor isn't currently the active view, so we'll assume that it
    // was updated from a drag and drop operation.
    UpdateTemplateName();
  }
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
  gfx::RectF name_view_bounds(name_view_->GetMirroredBounds());
  views::View::ConvertRectToTarget(name_view_->parent(), this,
                                   &name_view_bounds);

  // With the design of the template card having the textfield within a
  // clickable button, as well as having the grid view be a `PreTargetHandler`,
  // we needed to make `this` a `ViewTargeterDelegate` for the view event
  // targeter in order to allow the `name_view_` to be specifically targeted and
  // focused. Use the centerpoint for `rect` as parts of `rect` may be outside
  // the `name_view_bounds` for touch events.
  if (root == this &&
      gfx::ToRoundedRect(name_view_bounds).Contains(rect.CenterPoint())) {
    return name_view_;
  }
  return views::ViewTargeterDelegate::TargetForRect(root, rect);
}

DesksTemplatesItemView* DesksTemplatesItemView::FindOtherTemplateWithName(
    const std::u16string& name) const {
  const auto templates_grid_view_items =
      static_cast<const DesksTemplatesGridView*>(parent())->grid_items();

  auto iter = std::find_if(
      templates_grid_view_items.begin(), templates_grid_view_items.end(),
      [this, name](const DesksTemplatesItemView* d) {
        return (d != this && d->desk_template()->template_name() == name);
      });
  return iter == templates_grid_view_items.end() ? nullptr : *iter;
}

void DesksTemplatesItemView::OnDeleteTemplate() {
  DesksTemplatesPresenter::Get()->DeleteEntry(
      desk_template_->uuid().AsLowercaseString());
}

void DesksTemplatesItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->ShowDeleteDialog(
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      name_view_->GetAccessibleName(),
      base::BindOnce(&DesksTemplatesItemView::OnDeleteTemplate,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DesksTemplatesItemView::OnGridItemPressed(const ui::Event& event) {
  MaybeLaunchTemplate(event.IsShiftDown());
}

void DesksTemplatesItemView::MaybeLaunchTemplate(bool should_delay) {
  if (is_template_name_being_modified_) {
    DesksTemplatesNameView::CommitChanges(GetWidget());
    return;
  }

  // Make shift-click on the launch button launch apps with a delay. This allows
  // developers to simulate delayed launch behaviors with ARC apps.
  // TODO(crbug.com/1281685): Remove before feature launch.
  base::TimeDelta delay;
#if !defined(OFFICIAL_BUILD)
  if (should_delay)
    delay = base::Seconds(3);
#endif

  DesksTemplatesPresenter::Get()->LaunchDeskTemplate(
      desk_template_->uuid().AsLowercaseString(), delay,
      GetWidget()->GetNativeWindow()->GetRootWindow());
}

void DesksTemplatesItemView::OnTemplateNameChanged(
    const std::u16string& new_name) {
  if (is_template_name_being_modified_)
    return;

  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  name_view_->ResetTemporaryName();
  SetAccessibleName(new_name);

  Layout();
}

views::View* DesksTemplatesItemView::GetView() {
  return this;
}

void DesksTemplatesItemView::MaybeActivateHighlightedView() {
  MaybeLaunchTemplate(/*should_delay=*/false);
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
