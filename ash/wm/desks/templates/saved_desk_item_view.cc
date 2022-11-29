// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desks_textfield.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// The padding values of the SavedDeskItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// The corner radius for the SavedDeskItemView.
constexpr int kCornerRadius = 16;

// The margin for the delete button.
constexpr int kDeleteButtonMargin = 8;

// The distance from the bottom of the launch button to the bottom of `this`.
constexpr int kLaunchButtonDistanceFromBottomDp = 14;

// The preferred width of the container that houses the template name textfield
// and managed status indicator and the time label.
constexpr int kTemplateNameAndTimePreferredWidth =
    SavedDeskItemView::kPreferredSize.width() - kHorizontalPaddingDp * 2;

// The height of the view which contains the time of the template.
constexpr int kTimeViewHeight = 24;

// The spacing between the textfield and the managed status icon.
constexpr int kManagedStatusIndicatorSpacing = 8;
constexpr int kManagedStatusIndicatorSize = 20;

// There is a gap between the background of the name view and the name view's
// actual text.
constexpr auto kTemplateNameInsets = gfx::Insets::VH(0, 2);

// The time duration for the hover and icon containers to fade in and out.
constexpr int kFadeDurationMs = 100;

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

SavedDeskItemView::SavedDeskItemView(
    std::unique_ptr<DeskTemplate> desk_template)
    : desk_template_(std::move(desk_template)) {
  auto launch_template_callback = base::BindRepeating(
      &SavedDeskItemView::OnGridItemPressed, weak_ptr_factory_.GetWeakPtr());

  const std::u16string template_name = desk_template_->template_name();
  DCHECK(!template_name.empty());
  auto* color_provider = AshColorProvider::Get();
  const bool is_admin_managed =
      desk_template_->source() == DeskTemplateSource::kPolicy;

  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  views::Builder<SavedDeskItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(template_name)
      .SetCallback(std::move(launch_template_callback))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kColorAshShieldAndBase80, kCornerRadius))
      .SetBorder(std::make_unique<views::HighlightBorder>(
          kCornerRadius, views::HighlightBorder::Type::kHighlightBorder1,
          /*use_light_colors=*/false))
      .AddChildren(
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .SetInteriorMargin(
                  gfx::Insets::VH(kVerticalPaddingDp, kHorizontalPaddingDp))
              // TODO(richui): Consider splitting some of the children into
              // different files and/or classes.
              .AddChildren(
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kHorizontal)
                      .SetPreferredSize(gfx::Size(
                          kTemplateNameAndTimePreferredWidth,
                          SavedDeskNameView::kSavedDeskNameViewHeight))
                      .AddChildren(
                          views::Builder<SavedDeskNameView>()
                              .CopyAddressTo(&name_view_)
                              .SetController(this)
                              .SetText(template_name)
                              .SetAccessibleName(template_name)
                              .SetReadOnly(!desk_template_->IsModifiable())
                              // Use the focus behavior specified by the
                              // subclass of `SavedDeskNameView` unless the
                              // template is not modifiable.
                              .SetFocusBehavior(desk_template_->IsModifiable()
                                                    ? GetFocusBehavior()
                                                    : FocusBehavior::NEVER)
                              .SetProperty(
                                  views::kFlexBehaviorKey,
                                  views::FlexSpecification(
                                      views::MinimumFlexSizeRule::kScaleToZero,
                                      views::MaximumFlexSizeRule::kPreferred)),
                          // This is a spacer between the name field and the
                          // "managed-by-admin" admin icon.
                          views::Builder<views::View>()
                              .SetPreferredSize(
                                  gfx::Size(kManagedStatusIndicatorSpacing, 1))
                              .SetProperty(
                                  views::kFlexBehaviorKey,
                                  views::FlexSpecification(
                                      views::MinimumFlexSizeRule::kPreferred,
                                      views::MaximumFlexSizeRule::kPreferred))
                              .SetVisible(is_admin_managed),
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
                              .SetProperty(
                                  views::kFlexBehaviorKey,
                                  views::FlexSpecification(
                                      views::MinimumFlexSizeRule::kPreferred,
                                      views::MaximumFlexSizeRule::kPreferred))
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
                  views::Builder<SavedDeskIconContainer>()
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

  const int button_text_id =
      desk_template_->type() == DeskTemplateType::kTemplate
          ? IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON
          : IDS_ASH_DESKS_TEMPLATES_OPEN_DESK_BUTTON;
  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SavedDeskItemView::OnGridItemPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(button_text_id),
      PillButton::Type::kDefaultWithoutIcon,
      /*icon=*/nullptr));

  // Users cannot delete admin templates.
  if (!is_admin_managed) {
    delete_button_ =
        hover_container_->AddChildView(std::make_unique<CloseButton>(
            base::BindRepeating(&SavedDeskItemView::OnDeleteButtonPressed,
                                weak_ptr_factory_.GetWeakPtr()),
            CloseButton::Type::kMedium, &kDeleteIcon,
            kColorAshControlBackgroundColorInactive));
    delete_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON));
  }

  // Use a border to create spacing between `name_view_`s background (set in
  // `DesksTextfield`) and the actual text. Shift the parent by the same amount
  // so that the text stays aligned with the text in `time_view`. We shift the
  // parent here and not `name_view_` itself otherwise its bounds will be
  // outside the parent bounds and the background will get clipped.
  name_view_->SetBorder(views::CreateEmptyBorder(kTemplateNameInsets));
  name_view_->parent()->SetProperty(views::kMarginsKey, -kTemplateNameInsets);
  name_view_observation_.Observe(name_view_);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<SavedDeskItemView*>(view)->IsViewHighlighted();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  hover_container_->SetPaintToLayer();
  icon_container_view_->SetPaintToLayer();

  hover_container_->layer()->SetFillsBoundsOpaquely(false);
  icon_container_view_->layer()->SetFillsBoundsOpaquely(false);
}

SavedDeskItemView::~SavedDeskItemView() {
  name_view_observation_.Reset();
}

void SavedDeskItemView::UpdateHoverButtonsVisibility(
    const gfx::Point& screen_location,
    bool is_touch) {
  gfx::Point location_in_view = screen_location;
  ConvertPointFromScreen(this, &location_in_view);

  // For switch access, setting the hover buttons to visible allows users to
  // navigate to it.
  bool previous_hover_container_visibility = hover_container_should_be_visible_;
  hover_container_should_be_visible_ =
      !is_template_name_being_modified_ &&
      ((is_touch && HitTestPoint(location_in_view)) ||
       (!is_touch && IsMouseHovered()) ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());

  if (previous_hover_container_visibility ==
      hover_container_should_be_visible_) {
    return;
  }

  if (hover_container_should_be_visible_) {
    hover_container_->SetVisible(true);
    AnimateHover(hover_container_->layer(), icon_container_view_->layer());
  } else {
    icon_container_view_->SetVisible(true);
    AnimateHover(icon_container_view_->layer(), hover_container_->layer());
  }
}

bool SavedDeskItemView::IsNameBeingModified() const {
  return name_view_->HasFocus();
}

void SavedDeskItemView::SetDisplayName(const std::u16string& saved_desk_name) {
  name_view_->SetTemporaryName(saved_desk_name);
  name_view_->SetViewName(saved_desk_name);
}

void SavedDeskItemView::MaybeShowReplaceDialog(DeskTemplateType type,
                                               const base::GUID& uuid) {
  // Show replace template dialog. If accepted, replace old template and commit
  // name change.
  auto* controller = saved_desk_util::GetSavedDeskDialogController();
  if (!controller)
    return;

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  controller->ShowReplaceDialog(
      root_window, name_view_->GetText(), type,
      base::BindOnce(&SavedDeskItemView::ReplaceTemplate,
                     weak_ptr_factory_.GetWeakPtr(), uuid),
      base::BindOnce(&SavedDeskItemView::RevertTemplateName,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskItemView::ReplaceTemplate(const base::GUID& uuid) {
  // Make sure we delete the template we are replacing first, so that we don't
  // get template name collisions. Passing `nullopt` as `record_for_type` since
  // we only record the delete operation when the user specifically deletes an
  // entry.
  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->DeleteEntry(uuid, /*record_for_type=*/absl::nullopt);
    UpdateTemplateName();
    RecordReplaceSavedDeskHistogram(desk_template_->type());
  }
}

void SavedDeskItemView::RevertTemplateName() {
  views::FocusManager* focus_manager = GetFocusManager();
  focus_manager->SetFocusedView(name_view_);
  const auto temporary_name = name_view_->temporary_name();
  name_view_->SetViewName(
      temporary_name.value_or(desk_template_->template_name()));
  name_view_->SelectAll(true);

  name_view_->OnContentsChanged();
}

void SavedDeskItemView::UpdateTemplate(const DeskTemplate& updated_template) {
  desk_template_ = updated_template.Clone();

  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);

  auto new_name = desk_template_->template_name();
  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  SetAccessibleName(new_name);

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`
  name_view_->OnContentsChanged();
}

void SavedDeskItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  int accessible_text_id =
      desk_template_->type() == DeskTemplateType::kTemplate
          ? IDS_ASH_DESKS_TEMPLATES_LIBRARY_TEMPLATES_GRID_ITEM_ACCESSIBLE_NAME
          : IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVE_AND_RECALL_GRID_ITEM_ACCESSIBLE_NAME;

  node_data->role = ax::mojom::Role::kButton;

  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kName,
      l10n_util::GetStringFUTF8(accessible_text_id,
                                desk_template_->template_name()));

  node_data->AddStringAttribute(
      ax::mojom::StringAttribute::kDescription,
      l10n_util::GetStringUTF8(
          IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVED_DESK_GRID_ITEM_EXTRA_ACCESSIBLE_DESCRIPTION));
}

void SavedDeskItemView::Layout() {
  views::View::Layout();

  if (delete_button_) {
    const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
    DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
    delete_button_->SetBoundsRect(
        gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                  kDeleteButtonMargin, delete_button_size.width(),
                  delete_button_size.height()));
  }

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize();
  launch_button_->SetBoundsRect(
      gfx::Rect({(width() - launch_button_preferred_size.width()) / 2,
                 height() - launch_button_preferred_size.height() -
                     kLaunchButtonDistanceFromBottomDp},
                launch_button_preferred_size));
}

void SavedDeskItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();

  time_view_->SetBackgroundColor(SK_ColorTRANSPARENT);
  time_view_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
}

void SavedDeskItemView::OnViewFocused(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about focus on
  // `name_view_`.
  if (observed_view == this)
    return;

  DCHECK_EQ(observed_view, name_view_);

  // Make sure the current desk item view is fully visible.
  ScrollViewToVisible();

  is_template_name_being_modified_ = true;

  // Assume we should commit the name change unless `HandleKeyEvent` detects the
  // user pressed the escape key.
  should_commit_name_changes_ = true;

  // Hide the hover container when we are modifying the template name.
  hover_container_->SetVisible(false);
  icon_container_view_->SetVisible(true);
  hover_container_->layer()->SetOpacity(0.0f);
  icon_container_view_->layer()->SetOpacity(1.0f);

  // Set the Overview highlight to move focus with the `name_view_`.
  auto* highlight_controller = Shell::Get()
                                   ->overview_controller()
                                   ->overview_session()
                                   ->highlight_controller();
  if (highlight_controller->IsFocusHighlightVisible()) {
    highlight_controller->MoveHighlightToView(name_view_);

    // Update a11y focus window.
    highlight_controller->UpdateA11yFocusWindow(name_view_);
  }

  if (!defer_select_all_)
    name_view_->SelectAll(false);
}

void SavedDeskItemView::OnViewBlurred(views::View* observed_view) {
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
    // Saving a desk template always puts it in the top left corner of the desk
    // templates grid. This may mean that the grid is no longer sorted
    // alphabetically by template name. Ensure that the grid is sorted.
    for (auto& overview_grid : overview_session->grid_list()) {
      if (SavedDeskLibraryView* library_view =
              overview_grid->GetSavedDeskLibraryView()) {
        for (auto* grid_view : library_view->grid_views()) {
          grid_view->SortEntries(/*order_first_uuid=*/{});
        }
      }
    }
    return;
  }

  // Check if template name exist, replace existing template if confirmed by
  // user. Use a post task to avoid activating a widget while another widget is
  // still being activated. In this case, we don't want to show the dialog and
  // activate its associated widget until after the desks bar widget is finished
  // activating. See https://crbug.com/1301759.
  auto* presenter = saved_desk_util::GetSavedDeskPresenter();
  if (!presenter)
    return;

  auto* template_to_replace = presenter->FindOtherEntryWithName(
      name_view_->GetText(), desk_template().type(), uuid());
  if (template_to_replace) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SavedDeskItemView::MaybeShowReplaceDialog,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  template_to_replace->type(),
                                  template_to_replace->uuid()));
    return;
  }

  UpdateTemplateName();
}

void SavedDeskItemView::OnFocus() {
  UpdateOverviewHighlightForFocusAndSpokenFeedback(this);
  OnViewHighlighted();
  View::OnFocus();
}

void SavedDeskItemView::OnBlur() {
  OnViewUnhighlighted();
  View::OnBlur();
}

views::Button::KeyClickAction SavedDeskItemView::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  // Prevents any key events from activating a button click while the template
  // name is being modified.
  if (is_template_name_being_modified_)
    return KeyClickAction::kNone;

  return Button::GetKeyClickActionForEvent(event);
}

void SavedDeskItemView::UpdateTemplateName() {
  desk_template_->set_template_name(name_view_->GetText());
  OnTemplateNameChanged(desk_template_->template_name());

  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->SaveOrUpdateDeskTemplate(
        /*is_update=*/true, GetWidget()->GetNativeWindow()->GetRootWindow(),
        desk_template_->Clone());
  }
}

void SavedDeskItemView::OnHoverAnimationEnded() {
  hover_container_->SetVisible(hover_container_should_be_visible_);
  icon_container_view_->SetVisible(!hover_container_should_be_visible_);
}

void SavedDeskItemView::AnimateHover(ui::Layer* layer_to_show,
                                     ui::Layer* layer_to_hide) {
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<SavedDeskItemView> view) {
            if (view)
              view->OnHoverAnimationEnded();
          },
          weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetOpacity(layer_to_show, 0.0f)
      .SetOpacity(layer_to_hide, 1.0f)
      .Then()
      .SetDuration(base::Milliseconds(kFadeDurationMs))
      .SetOpacity(layer_to_show, 1.0f)
      .SetOpacity(layer_to_hide, 0.0f);
}

void SavedDeskItemView::ContentsChanged(views::Textfield* sender,
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

bool SavedDeskItemView::HandleKeyEvent(views::Textfield* sender,
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

  SavedDeskNameView::CommitChanges(GetWidget());

  return true;
}

bool SavedDeskItemView::HandleMouseEvent(views::Textfield* sender,
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

views::View* SavedDeskItemView::TargetForRect(views::View* root,
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

void SavedDeskItemView::OnDeleteTemplate() {
  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter())
    presenter->DeleteEntry(desk_template_->uuid(), desk_template_->type());
}

void SavedDeskItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* controller = saved_desk_util::GetSavedDeskDialogController();
  if (!controller)
    return;

  controller->ShowDeleteDialog(
      GetWidget()->GetNativeWindow()->GetRootWindow(),
      name_view_->GetAccessibleName(), desk_template_->type(),
      base::BindOnce(&SavedDeskItemView::OnDeleteTemplate,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskItemView::OnGridItemPressed(const ui::Event& event) {
  MaybeLaunchTemplate();
}

void SavedDeskItemView::MaybeLaunchTemplate() {
  if (is_template_name_being_modified_) {
    SavedDeskNameView::CommitChanges(GetWidget());
    return;
  }

  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->LaunchSavedDesk(desk_template_->Clone(),
                               GetWidget()->GetNativeWindow()->GetRootWindow());
  }
}

void SavedDeskItemView::OnTemplateNameChanged(const std::u16string& new_name) {
  if (is_template_name_being_modified_)
    return;

  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  name_view_->SetAccessibleName(new_name);
  name_view_->ResetTemporaryName();
  SetAccessibleName(new_name);

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`.
  name_view_->OnContentsChanged();
}

views::View* SavedDeskItemView::GetView() {
  return this;
}

void SavedDeskItemView::MaybeActivateHighlightedView() {
  MaybeLaunchTemplate();
}

void SavedDeskItemView::MaybeCloseHighlightedView(bool primary_action) {
  if (primary_action)
    OnDeleteButtonPressed();
}

void SavedDeskItemView::MaybeSwapHighlightedView(bool right) {}

void SavedDeskItemView::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();

  ScrollViewToVisible();
}

void SavedDeskItemView::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(SavedDeskItemView, views::Button)
END_METADATA

}  // namespace ash
