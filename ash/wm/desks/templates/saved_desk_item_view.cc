// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/desk_textfield.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_icon_container.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/wm_constants.h"
#include "base/i18n/time_formatting.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// The padding values of the SavedDeskItemView.
constexpr int kVerticalPaddingDp = 14;

// The margin for the delete button.
constexpr int kDeleteButtonMargin = 8;

// The distance from the bottom of the launch button to the bottom of `this`.
constexpr int kLaunchButtonDistanceFromBottomDp = 14;

// The preferred width of the container that houses the saved desk name
// textfield and managed status indicator and the time label.
constexpr int kSavedDeskNameAndTimePreferredWidth =
    SavedDeskItemView::kPreferredSize.width() - kSaveDeskPaddingDp * 2;

// The height of the view which contains the time of the saved desk.
constexpr int kTimeViewHeight = 24;

// The size of the managed status icon.
constexpr int kManagedStatusIndicatorSize = 20;

// There is a gap between the background of the name view and the name view's
// actual text.
constexpr auto kSavedDeskNameInsets = gfx::Insets::VH(0, 2);

// The time duration for the hover and icon containers to fade in and out.
constexpr int kFadeDurationMs = 100;

std::u16string GetTimeStr(base::Time timestamp) {
  // `ui::TimeFormat::RelativeDate()` returns an empty string if `timestamp` is
  // out of relative date range, which is yesterday and today as of now.
  const std::u16string date = ui::TimeFormat::RelativeDate(timestamp, nullptr);
  return date.empty()
             // Syntax `yMMMdjmm` is used by the File App if it's not a relative
             // date. Please note, this might be slightly different for
             // different locales. Examples:
             //  `en-US` - `Jan 1, 2022, 10:30 AM`
             //  `zh-CN` - `2022年1月1日 10:30`
             ? base::LocalizedTimeFormatWithPattern(timestamp, "yMMMdjmm")
             // If it's a relative date, just append `jmm` to it.
             // Please note, this might be slightly different for different
             // locales. Examples:
             //  `en-US` - `Today 10:30 AM`
             //  `zh-CN` - `今天 10:30`
             : (date + u" " +
                base::LocalizedTimeFormatWithPattern(timestamp, "jmm"));
}

}  // namespace

SavedDeskItemView::SavedDeskItemView(std::unique_ptr<DeskTemplate> saved_desk)
    : saved_desk_(std::move(saved_desk)) {
  auto launch_template_callback = base::BindRepeating(
      &SavedDeskItemView::OnGridItemPressed, weak_ptr_factory_.GetWeakPtr());

  const std::u16string saved_desk_name = saved_desk_->template_name();
  DCHECK(!saved_desk_name.empty());
  const bool is_admin_managed =
      saved_desk_->source() == DeskTemplateSource::kPolicy;
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  View* background_view = nullptr;
  View* box_layout_view = nullptr;
  views::Builder<SavedDeskItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(ComputeAccessibleName())
      .SetCallback(std::move(launch_template_callback))
      .AddChildren(
          views::Builder<View>()
              .CopyAddressTo(&background_view)
              .SetPreferredSize(kPreferredSize)
              .SetUseDefaultFillLayout(true)
              .SetBackground(views::CreateThemedRoundedRectBackground(
                  cros_tokens::kCrosSysSystemBaseElevated,
                  kSaveDeskCornerRadius)),
          views::Builder<views::FlexLayoutView>()
              .SetOrientation(views::LayoutOrientation::kVertical)
              .CopyAddressTo(&box_layout_view)
              .SetInteriorMargin(
                  gfx::Insets::VH(kVerticalPaddingDp, kSaveDeskPaddingDp))
              // TODO(richui): Consider splitting some of the children into
              // different files and/or classes.
              .AddChildren(
                  views::Builder<views::FlexLayoutView>()
                      .SetOrientation(views::LayoutOrientation::kHorizontal)
                      .AddChildren(
                          views::Builder<SavedDeskNameView>()
                              .CopyAddressTo(&name_view_)
                              .SetController(this)
                              .SetText(saved_desk_name)
                              .SetAccessibleName(l10n_util::GetStringUTF16(
                                  IDS_ASH_DESKS_DESK_NAME))
                              .SetReadOnly(!saved_desk_->IsModifiable())
                              // Use the focus behavior specified by the
                              // subclass of `SavedDeskNameView` unless the
                              // saved desk is not modifiable.
                              .SetFocusBehavior(saved_desk_->IsModifiable()
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
                                  gfx::Size(kSaveDeskSpacingDp, 1))
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
                              .SetImage(ui::ImageModel::FromVectorIcon(
                                  chromeos::kEnterpriseIcon,
                                  cros_tokens::kCrosSysSecondary,
                                  kManagedStatusIndicatorSize))
                              .SetProperty(
                                  views::kFlexBehaviorKey,
                                  views::FlexSpecification(
                                      views::MinimumFlexSizeRule::kPreferred,
                                      views::MaximumFlexSizeRule::kPreferred))
                              .SetVisible(is_admin_managed)),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetEnabledColorId(cros_tokens::kCrosSysSecondary)
                      .SetText(
                          is_admin_managed
                              ? l10n_util::GetStringUTF16(
                                    IDS_ASH_DESKS_TEMPLATES_MANAGEMENT_STATUS_DESCRIPTION)
                              : GetTimeStr(saved_desk_->created_time()))
                      .SetPreferredSize(
                          gfx::Size(kSavedDeskNameAndTimePreferredWidth,
                                    kTimeViewHeight)),
                  // View which acts as a spacer, taking up all the available
                  // space between the date and the icons container.
                  views::Builder<views::View>().SetProperty(
                      views::kFlexBehaviorKey,
                      views::FlexSpecification(
                          views::MinimumFlexSizeRule::kScaleToZero,
                          views::MaximumFlexSizeRule::kUnbounded)),
                  views::Builder<SavedDeskIconContainer>()
                      .CopyAddressTo(&icon_container_view_)
                      .PopulateIconContainerFromTemplate(saved_desk_.get())
                      .SetVisible(true)),
          views::Builder<views::View>()
              .CopyAddressTo(&hover_container_)
              .SetUseDefaultFillLayout(true)
              .SetVisible(true))
      .BuildChildren();

  SetPaintToLayer();
  // We need to ensure that the layer is non-opaque when animating.
  layer()->SetFillsBoundsOpaquely(false);

  // Create a shadow for the view.
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayerForView(
      this, SystemShadow::Type::kElevation12);
  shadow_->SetRoundedCornerRadius(kSaveDeskCornerRadius);

  if (features::IsBackgroundBlurEnabled()) {
    background_view->SetPaintToLayer();
    background_view->layer()->SetFillsBoundsOpaquely(false);
    background_view->layer()->SetBackgroundBlur(
        ColorProvider::kBackgroundBlurSigma);
    background_view->layer()->SetBackdropFilterQuality(
        ColorProvider::kBackgroundBlurQuality);
    background_view->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(kSaveDeskCornerRadius));

    // This needs to be painted to a layer if its sibling `background_view` is.
    // Otherwise, it will be painted to its ancestors layer and
    // `background_view` will be drawn on top of it as a result.
    box_layout_view->SetPaintToLayer();
    box_layout_view->layer()->SetFillsBoundsOpaquely(false);
  }

  const int button_text_id = saved_desk_->type() == DeskTemplateType::kTemplate
                                 ? IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON
                                 : IDS_ASH_DESKS_TEMPLATES_OPEN_DESK_BUTTON;
  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SavedDeskItemView::OnGridItemPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      l10n_util::GetStringUTF16(button_text_id),
      PillButton::Type::kDefaultWithoutIcon,
      /*icon=*/nullptr));
  launch_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  // Users cannot delete admin templates.
  if (!is_admin_managed) {
    delete_button_ =
        hover_container_->AddChildView(std::make_unique<IconButton>(
            base::BindRepeating(&SavedDeskItemView::OnDeleteButtonPressed,
                                weak_ptr_factory_.GetWeakPtr()),
            IconButton::Type::kXSmall, &kDeleteIcon,
            /*is_togglable=*/false,
            /*has_border=*/false));
    delete_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_DESKS_TEMPLATES_DELETE_DIALOG_CONFIRM_BUTTON));
    delete_button_->SetFocusBehavior(
        views::View::FocusBehavior::ACCESSIBLE_ONLY);
  }

  // Use a border to create spacing between `name_view_`s background (set in
  // `DeskTextfield`) and the actual text. Shift the parent by the same amount
  // so that the text stays aligned with the text in `time_view`. We shift the
  // parent here and not `name_view_` itself otherwise its bounds will be
  // outside the parent bounds and the background will get clipped.
  name_view_->SetBorder(views::CreateEmptyBorder(kSavedDeskNameInsets));
  name_view_->parent()->SetProperty(views::kMarginsKey, -kSavedDeskNameInsets);
  name_view_observation_.Observe(name_view_);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kSaveDeskCornerRadius);

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kWindowMiniViewFocusRingHaloInset);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  hover_container_->SetPaintToLayer();
  icon_container_view_->SetPaintToLayer();

  hover_container_->layer()->SetFillsBoundsOpaquely(false);
  icon_container_view_->layer()->SetFillsBoundsOpaquely(false);

  hover_container_->layer()->SetOpacity(0.0f);
  icon_container_view_->layer()->SetOpacity(1.0f);

  AddAccelerator(ui::Accelerator(ui::VKEY_W, ui::EF_CONTROL_DOWN));

  GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  GetViewAccessibility().SetDescription(l10n_util::GetStringUTF8(
      IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVED_DESK_GRID_ITEM_EXTRA_ACCESSIBLE_DESCRIPTION));
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
      !is_saved_desk_name_being_modified_ &&
      ((is_touch && HitTestPoint(location_in_view)) ||
       (!is_touch && IsMouseHovered()) ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning());

  if (previous_hover_container_visibility ==
      hover_container_should_be_visible_) {
    return;
  }

  if (hover_container_should_be_visible_) {
    AnimateHover(hover_container_->layer(), icon_container_view_->layer());
  } else {
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
                                               const base::Uuid& uuid) {
  // Show replace saved desk dialog. If accepted, replace old saved desk item
  // and commit name change.
  auto* controller = saved_desk_util::GetSavedDeskDialogController();
  if (!controller)
    return;

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  controller->ShowReplaceDialog(
      root_window, name_view_->GetText(), type,
      base::BindOnce(&SavedDeskItemView::ReplaceSavedDesk,
                     weak_ptr_factory_.GetWeakPtr(), uuid),
      base::BindOnce(&SavedDeskItemView::RevertSavedDeskName,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskItemView::ReplaceSavedDesk(const base::Uuid& uuid) {
  // Make sure we delete the saved desk we are replacing first, so that we don't
  // get saved desk name collisions. Passing `nullopt` as `record_for_type`
  // since we only record the delete operation when the user specifically
  // deletes an entry.
  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->DeleteEntry(uuid, /*record_for_type=*/std::nullopt);
    UpdateSavedDeskName();
    RecordReplaceSavedDeskHistogram(saved_desk_->type());
  }
}

void SavedDeskItemView::RevertSavedDeskName() {
  views::FocusManager* focus_manager = GetFocusManager();
  focus_manager->SetFocusedView(name_view_);
  const auto temporary_name = name_view_->temporary_name();
  name_view_->SetViewName(
      temporary_name.value_or(saved_desk_->template_name()));
  name_view_->SelectAll(true);

  name_view_->OnContentsChanged();
}

void SavedDeskItemView::UpdateSavedDesk(
    const DeskTemplate& updated_saved_desk) {
  saved_desk_ = updated_saved_desk.Clone();

  auto new_name = saved_desk_->template_name();
  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  GetViewAccessibility().SetName(ComputeAccessibleName());

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`
  name_view_->OnContentsChanged();
}

void SavedDeskItemView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  if (delete_button_) {
    const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
    DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
    delete_button_->SetBoundsRect(
        gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                  kDeleteButtonMargin, delete_button_size.width(),
                  delete_button_size.height()));
  }

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize({});
  launch_button_->SetBoundsRect(
      gfx::Rect({(width() - launch_button_preferred_size.width()) / 2,
                 height() - launch_button_preferred_size.height() -
                     kLaunchButtonDistanceFromBottomDp},
                launch_button_preferred_size));
}

void SavedDeskItemView::OnViewFocused(views::View* observed_view) {
  // `this` is a button which observes itself. Here we only care about focus on
  // `name_view_`.
  if (observed_view == this)
    return;

  DCHECK_EQ(observed_view, name_view_);

  // Make sure the current saved desk item view is fully visible.
  ScrollViewToVisible();

  is_saved_desk_name_being_modified_ = true;

  // Assume we should commit the name change unless `HandleKeyEvent` detects the
  // user pressed the escape key.
  should_commit_name_changes_ = true;

  // Hide the hover container when we are modifying the saved desk name.
  hover_container_->layer()->SetOpacity(0.0f);
  icon_container_view_->layer()->SetOpacity(1.0f);

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
  // TODO(richui): Revisit this once the behavior of the saved desk name when
  // exiting overview is determined.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  if (!overview_session || overview_session->is_shutting_down())
    return;

  DCHECK_EQ(observed_view, name_view_);
  is_saved_desk_name_being_modified_ = false;
  defer_select_all_ = false;

  // Collapse the whitespace for the text first before comparing it or trying to
  // commit the name in order to prevent duplicate name issues.
  const std::u16string user_entered_name =
      base::CollapseWhitespace(name_view_->GetText(),
                               /*trim_sequences_with_line_breaks=*/false);
  name_view_->SetText(user_entered_name);

  // When committing the name, do not allow an empty saved desk name. Also,
  // don't commit the name changes if the view was blurred from the user
  // pressing the escape key (identified by `should_commit_name_changes_`).
  // Revert back to the original name.
  if (!should_commit_name_changes_ || user_entered_name.empty() ||
      saved_desk_->template_name() == user_entered_name) {
    OnSavedDeskNameChanged(saved_desk_->template_name());
    // Saving a saved desk always puts it in the top left corner of the
    // saved desks grid. This may mean that the grid is no longer sorted
    // alphabetically by saved desk name. Ensure that the grid is sorted.
    for (auto& overview_grid : overview_session->grid_list()) {
      if (SavedDeskLibraryView* library_view =
              overview_grid->GetSavedDeskLibraryView()) {
        for (ash::SavedDeskGridView* grid_view : library_view->grid_views()) {
          grid_view->SortEntries(/*order_first_uuid=*/{});
        }
      }
    }
    return;
  }

  // Check if the saved desk name exist, replace existing saved desk if
  // confirmed by user. Use a post task to avoid activating a widget while
  // another widget is still being activated. In this case, we don't want to
  // show the dialog and activate its associated widget until after the desks
  // bar widget is finished activating. See https://crbug.com/1301759.
  auto* presenter = saved_desk_util::GetSavedDeskPresenter();
  if (!presenter)
    return;

  auto* saved_desk_entry_to_replace = presenter->FindOtherEntryWithName(
      name_view_->GetText(), saved_desk().type(), uuid());
  if (saved_desk_entry_to_replace) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SavedDeskItemView::MaybeShowReplaceDialog,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  saved_desk_entry_to_replace->type(),
                                  saved_desk_entry_to_replace->uuid()));
    return;
  }

  UpdateSavedDeskName();
}

views::Button::KeyClickAction SavedDeskItemView::GetKeyClickActionForEvent(
    const ui::KeyEvent& event) {
  // Prevents any key events from activating a button click while the saved desk
  // name is being modified.
  if (is_saved_desk_name_being_modified_) {
    return KeyClickAction::kNone;
  }

  return Button::GetKeyClickActionForEvent(event);
}

bool SavedDeskItemView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  if (accelerator.IsCtrlDown() && accelerator.key_code() == ui::VKEY_W) {
    OnDeleteButtonPressed();
    return true;
  }
  return views::Button::AcceleratorPressed(accelerator);
}

bool SavedDeskItemView::CanHandleAccelerators() const {
  return HasFocus() && views::Button::CanHandleAccelerators();
}

void SavedDeskItemView::UpdateSavedDeskName() {
  saved_desk_->set_template_name(name_view_->GetText());
  OnSavedDeskNameChanged(saved_desk_->template_name());

  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->SaveOrUpdateSavedDesk(
        /*is_update=*/true, GetWidget()->GetNativeWindow()->GetRootWindow(),
        saved_desk_->Clone());
  }
}

std::u16string SavedDeskItemView::ComputeAccessibleName() const {
  int accessible_text_id =
      saved_desk_->type() == DeskTemplateType::kTemplate
          ? IDS_ASH_DESKS_TEMPLATES_LIBRARY_TEMPLATES_GRID_ITEM_ACCESSIBLE_NAME
          : IDS_ASH_DESKS_TEMPLATES_LIBRARY_SAVE_AND_RECALL_GRID_ITEM_ACCESSIBLE_NAME;

  return l10n_util::GetStringFUTF16(accessible_text_id,
                                    saved_desk_->template_name());
}

void SavedDeskItemView::SetTooltipText(const std::u16string& tooltip_text) {
  NOTREACHED();
}

void SavedDeskItemView::AnimateHover(ui::Layer* layer_to_show,
                                     ui::Layer* layer_to_hide) {
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET)
      .Once()
      .SetDuration(base::Milliseconds(kFadeDurationMs))
      .SetOpacity(layer_to_show, 1.0f)
      .SetOpacity(layer_to_hide, 0.0f);
}

void SavedDeskItemView::ContentsChanged(views::Textfield* sender,
                                        const std::u16string& new_contents) {
  DCHECK_EQ(sender, name_view_);

  // To avoid potential security and memory issues, we don't allow saved desk
  // names to have an unbounded length. Therefore we trim if needed at
  // `kMaxLength` UTF-16 boundary. Note that we don't care about code point
  // boundaries in this case.
  if (new_contents.size() > DeskTextfield::kMaxLength) {
    std::u16string trimmed_new_contents = new_contents;
    trimmed_new_contents.resize(DeskTextfield::kMaxLength);
    name_view_->SetText(trimmed_new_contents);
  }

  name_view_->OnContentsChanged();

  auto* focus_manager = GetWidget()->GetFocusManager();
  if (focus_manager->GetFocusedView() != name_view_) {
    // The text editor isn't currently the active view, so we'll assume that it
    // was updated from a drag and drop operation.
    UpdateSavedDeskName();
  }
}

bool SavedDeskItemView::HandleKeyEvent(views::Textfield* sender,
                                       const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, name_view_);
  DCHECK(is_saved_desk_name_being_modified_);

  // Pressing enter or escape should blur the focus away from `name_view_` so
  // that editing the saved desk item's name ends. Pressing tab should do the
  // same, but is handled in `OverviewSession`.
  if (key_event.type() != ui::EventType::kKeyPressed) {
    return false;
  }

  if (key_event.key_code() != ui::VKEY_RETURN &&
      key_event.key_code() != ui::VKEY_ESCAPE) {
    return false;
  }

  // If the escape key was pressed, `should_commit_name_changes_` is set to
  // false so that `OnViewBlurred` knows that it should not change the name of
  // the saved desk.
  if (key_event.key_code() == ui::VKEY_ESCAPE)
    should_commit_name_changes_ = false;

  SavedDeskNameView::CommitChanges(GetWidget());

  return true;
}

bool SavedDeskItemView::HandleMouseEvent(views::Textfield* sender,
                                         const ui::MouseEvent& mouse_event) {
  DCHECK_EQ(sender, name_view_);

  switch (mouse_event.type()) {
    case ui::EventType::kMousePressed:
      // If this is the first mouse press on the `name_view_`, then it's not
      // focused yet. `OnViewFocused()` should not select all text, since it
      // will be undone by the mouse release event. Instead we defer it until we
      // get the mouse release event.
      if (!is_saved_desk_name_being_modified_) {
        defer_select_all_ = true;
      }
      break;

    case ui::EventType::kMouseReleased:
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

  // With the design of the saved desk card having the textfield within a
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

void SavedDeskItemView::OnDeleteSavedDesk() {
  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter())
    presenter->DeleteEntry(saved_desk_->uuid(), saved_desk_->type());
}

void SavedDeskItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* controller = saved_desk_util::GetSavedDeskDialogController();
  if (!controller)
    return;

  controller->ShowDeleteDialog(
      GetWidget()->GetNativeWindow()->GetRootWindow(), name_view_->GetText(),
      saved_desk_->type(),
      base::BindOnce(&SavedDeskItemView::OnDeleteSavedDesk,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SavedDeskItemView::OnGridItemPressed(const ui::Event& event) {
  MaybeLaunchSavedDesk();
}

void SavedDeskItemView::MaybeLaunchSavedDesk() {
  if (is_saved_desk_name_being_modified_) {
    SavedDeskNameView::CommitChanges(GetWidget());
    return;
  }

  if (auto* presenter = saved_desk_util::GetSavedDeskPresenter()) {
    presenter->LaunchSavedDesk(saved_desk_->Clone(),
                               GetWidget()->GetNativeWindow()->GetRootWindow());
  }
}

void SavedDeskItemView::OnSavedDeskNameChanged(const std::u16string& new_name) {
  if (is_saved_desk_name_being_modified_) {
    return;
  }

  DCHECK(!new_name.empty());
  name_view_->SetText(new_name);
  name_view_->ResetTemporaryName();
  GetViewAccessibility().SetName(ComputeAccessibleName());

  // This will trigger `name_view_` to compute its new preferred bounds and
  // invalidate the layout for `this`.
  name_view_->OnContentsChanged();
}

BEGIN_METADATA(SavedDeskItemView)
END_METADATA

}  // namespace ash
