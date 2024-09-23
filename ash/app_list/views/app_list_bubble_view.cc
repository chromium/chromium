// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_bubble_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_bubble_apps_collections_page.h"
#include "ash/app_list/views/app_list_bubble_apps_page.h"
#include "ash/app_list/views/app_list_bubble_search_page.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_search_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"
#include "ash/app_list/views/folder_background_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_dialog_controller.h"
#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"

using views::BoxLayout;

namespace ash {
namespace {

// Folder view inset from the edge of the bubble.
constexpr int kFolderViewInset = 16;

AppListConfig* GetAppListConfig() {
  return AppListConfigProvider::Get().GetConfigForType(
      AppListConfigType::kDense, /*can_create=*/true);
}

// Returns true if ChromeVox (spoken feedback) is enabled.
bool IsSpokenFeedbackEnabled() {
  return Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

// A simplified horizontal separator that uses a solid color layer for painting.
// This is more efficient than using a views::Separator, which would require
// SetPaintToLayer(ui::LAYER_TEXTURED).
class SeparatorWithLayer : public views::View {
  METADATA_HEADER(SeparatorWithLayer, views::View)

 public:
  SeparatorWithLayer() {
    SetPaintToLayer(ui::LAYER_SOLID_COLOR);
    // Color is set in OnThemeChanged().
    layer()->SetFillsBoundsOpaquely(false);
  }
  SeparatorWithLayer(const SeparatorWithLayer&) = delete;
  SeparatorWithLayer& operator=(const SeparatorWithLayer&) = delete;
  ~SeparatorWithLayer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // The parent's layout manager will stretch it horizontally.
    return gfx::Size(1, 1);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    layer()->SetColor(ColorProvider::Get()->GetContentLayerColor(
        ColorProvider::ContentLayerType::kSeparatorColor));
  }
};

BEGIN_METADATA(SeparatorWithLayer)
END_METADATA

// Returns the layer bounds to use for the start of the show animation and the
// end of the hide animation.
gfx::Rect GetShowHideAnimationBounds(bool is_side_shelf,
                                     gfx::Rect target_bounds) {
  // For either shelf: Height 75% → 100%
  const int delta_height = target_bounds.height() / 4;  // 25% of height
  const int initial_height = target_bounds.height() - delta_height;
  const int y_offset = 8;
  if (is_side_shelf) {
    // For side shelf: Y Position: Up 8px → End position, expanding down.
    return gfx::Rect(target_bounds.x(), target_bounds.y() - y_offset,
                     target_bounds.width(), initial_height);
  }
  // For bottom shelf: Y Position: Down 8px → End position, expanding up.
  return gfx::Rect(target_bounds.x(),
                   target_bounds.y() + delta_height + y_offset,
                   target_bounds.width(), initial_height);
}

const ui::DropTargetEvent GetTranslatedDropTargetEvent(
    const ui::DropTargetEvent event,
    views::View* src_view,
    views::View* dst_view) {
  gfx::Point event_location = event.location();
  views::View::ConvertPointToTarget(src_view, dst_view, &event_location);
  return ui::DropTargetEvent(event.data(), gfx::PointF(event_location),
                             event.root_location_f(),
                             event.source_operations());
}

}  // namespace

// Makes focus traversal skip the assistant button and the hide continue section
// button when pressing the down arrow key or the up arrow key. Normally views
// would move focus from the search box to the assistant button on arrow down.
// However, these buttons are visually to the right, so this feels weird.
// Likewise, on arrow up from continue tasks it feels better to put focus
// directly in the search box.
class ButtonFocusSkipper : public ui::EventHandler {
 public:
  ButtonFocusSkipper() { Shell::Get()->AddPreTargetHandler(this); }

  ~ButtonFocusSkipper() override { Shell::Get()->RemovePreTargetHandler(this); }

  void AddButton(views::View* button) {
    DCHECK(button);
    buttons_.push_back(button);
  }

  // ui::EventHandler:
  void OnEvent(ui::Event* event) override {
    // Don't adjust focus behavior if the user already focused the button.
    for (views::View* button : buttons_) {
      if (button->HasFocus()) {
        return;
      }
    }

    bool skip_focus = false;
    // This class overrides OnEvent() to examine all events so that focus
    // behavior is restored by mouse events, gesture events, etc.
    if (event->type() == ui::EventType::kKeyPressed) {
      ui::KeyboardCode key = event->AsKeyEvent()->key_code();
      if (key == ui::VKEY_UP || key == ui::VKEY_DOWN) {
        skip_focus = true;
      }
    }
    for (views::View* button : buttons_) {
      button->SetFocusBehavior(skip_focus ? views::View::FocusBehavior::NEVER
                                          : views::View::FocusBehavior::ALWAYS);
    }
  }

 private:
  std::vector<raw_ptr<views::View, VectorExperimental>> buttons_;
};

AppListBubbleView::AppListBubbleView(AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate);
  SetProperty(views::kElementIdentifierKey, kAppListBubbleViewElementId);

  const float corner_radius = GetBubbleCornerRadius();
  // Set up rounded corners and background blur, similar to TrayBubbleView.
  // Layer background is set in OnThemeChanged().
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{corner_radius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  ui::ColorId background_color_id = cros_tokens::kCrosSysSystemBaseElevated;
  SetBackground(views::CreateThemedRoundedRectBackground(background_color_id,
                                                         corner_radius));

  SetBorder(std::make_unique<views::HighlightBorder>(
      corner_radius, views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  SetLayoutManager(std::make_unique<views::FillLayout>());
  a11y_announcer_ = std::make_unique<AppListA11yAnnouncer>(
      AddChildView(std::make_unique<views::View>()));
  InitContentsView();

  // Add assistant page as a top-level child so it will fill the bubble and
  // suggestion chips will appear at the bottom of the bubble view.
  assistant_page_ = AddChildView(std::make_unique<AppListBubbleAssistantPage>(
      view_delegate_->GetAssistantViewDelegate()));
  assistant_page_->SetVisible(false);

  InitFolderView();
  // Folder view is laid out manually based on its contents.
  folder_view_->SetProperty(views::kViewIgnoredByLayoutKey, true);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));
}

AppListBubbleView::~AppListBubbleView() {
  // `a11y_announcer_` depends on a child view, so shut it down before view
  // hierarchy is destroyed.
  a11y_announcer_->Shutdown();

  // AppListFolderView may reference/observe an item on the root apps grid view
  // (associated with the folder), so destroy it before the root apps grid view.
  delete folder_view_;
  folder_view_ = nullptr;

  // Reset the dialog_controller for the AppsCollections page to prevent it from
  // dangling at destruction after the views are removed.
  apps_collections_page_->SetDialogController(nullptr);
}

void AppListBubbleView::UpdateSuggestions() {
  apps_page_->UpdateSuggestions();
}

void AppListBubbleView::InitContentsView() {
  auto* contents = AddChildView(std::make_unique<views::View>());

  auto* layout = contents->SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(BoxLayout::CrossAxisAlignment::kStretch);

  search_box_view_ = contents->AddChildView(std::make_unique<SearchBoxView>(
      /*delegate=*/this, view_delegate_, /*is_app_list_bubble=*/true));
  search_box_view_->InitializeForBubbleLauncher();

  // Skip the assistant button on arrow up/down in app list.
  button_focus_skipper_ = std::make_unique<ButtonFocusSkipper>();
  if (features::IsSunfishFeatureEnabled()) {
    button_focus_skipper_->AddButton(search_box_view_->sunfish_button());
  }
  button_focus_skipper_->AddButton(search_box_view_->assistant_button());

  // The main view has a solid color layer, so the separator needs its own
  // layer to visibly paint.
  separator_ = contents->AddChildView(std::make_unique<SeparatorWithLayer>());

  auto* pages_container =
      contents->AddChildView(std::make_unique<views::View>());

  // Apps page and search page must both fill the page for page transition
  // animations to look right.
  pages_container->SetUseDefaultFillLayout(true);

  // The apps page must fill the bubble so that the apps grid view can flex to
  // include empty space below the visible icons. The search page doesn't care,
  // so flex the entire container.
  layout->SetFlexForView(pages_container, 1);

  search_page_dialog_controller_ =
      std::make_unique<SearchResultPageDialogController>(search_box_view_);

  // NOTE: Passing drag and drop host from a specific shelf instance assumes
  // that the `apps_page_` will not get reused for showing the app list in
  // another root window.
  apps_page_ =
      pages_container->AddChildView(std::make_unique<AppListBubbleAppsPage>(
          view_delegate_, GetAppListConfig(), a11y_announcer_.get(),
          /*folder_controller=*/this,
          /*search_box=*/search_box_view_));

  apps_collections_page_ = pages_container->AddChildView(
      std::make_unique<AppListBubbleAppsCollectionsPage>(
          view_delegate_, GetAppListConfig(), a11y_announcer_.get(),
          search_page_dialog_controller_.get(),
          base::BindOnce(&AppListBubbleView::ShowPage,
                         weak_factory_.GetWeakPtr(),
                         AppListBubblePage::kApps)));

  // Skip the "hide continue section" button on arrow up/down in app list.
  button_focus_skipper_->AddButton(
      apps_page_->toggle_continue_section_button());

  search_page_ =
      pages_container->AddChildView(std::make_unique<AppListBubbleSearchPage>(
          view_delegate_, search_page_dialog_controller_.get(),
          search_box_view_));
  search_page_->SetVisible(false);
}

bool AppListBubbleView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  return apps_page_->scrollable_apps_grid_view()->GetDropFormats(formats,
                                                                 format_types);
}

bool AppListBubbleView::CanDrop(const OSExchangeData& data) {
  if (!apps_page_->GetVisible()) {
    return false;
  }

  return apps_page_->scrollable_apps_grid_view()->WillAcceptDropEvent(data);
}

void AppListBubbleView::OnDragExited() {
  apps_page_->scrollable_apps_grid_view()->OnDragExited();
}

void AppListBubbleView::OnDragEntered(const ui::DropTargetEvent& event) {
  AppsGridView* const scrollable_apps_grid =
      apps_page_->scrollable_apps_grid_view();

  scrollable_apps_grid->OnDragEntered(
      GetTranslatedDropTargetEvent(event, this, scrollable_apps_grid));
}

int AppListBubbleView::OnDragUpdated(const ui::DropTargetEvent& event) {
  AppsGridView* const scrollable_apps_grid =
      apps_page_->scrollable_apps_grid_view();

  return scrollable_apps_grid->OnDragUpdated(
      GetTranslatedDropTargetEvent(event, this, scrollable_apps_grid));
}

views::View::DropCallback AppListBubbleView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  AppsGridView* const scrollable_apps_grid =
      apps_page_->scrollable_apps_grid_view();

  return scrollable_apps_grid->GetDropCallback(
      GetTranslatedDropTargetEvent(event, this, scrollable_apps_grid));
}

void AppListBubbleView::InitFolderView() {
  auto folder_view = std::make_unique<AppListFolderView>(
      this, apps_page_->scrollable_apps_grid_view(), a11y_announcer_.get(),
      view_delegate_, /*tablet_mode=*/false);
  folder_view->UpdateAppListConfig(GetAppListConfig());
  folder_background_view_ =
      AddChildView(std::make_unique<FolderBackgroundView>(folder_view.get()));
  folder_background_view_->SetVisible(false);

  folder_view_ = AddChildView(std::move(folder_view));
  // Folder view will be set visible by its show animation.
  folder_view_->SetVisible(false);
}

void AppListBubbleView::StartShowAnimation(bool is_side_shelf) {
  // For performance, don't animate the shadow.
  view_shadow_.reset();

  // Ensure layout is up-to-date before animating views.
  if (needs_layout()) {
    DeprecatedLayoutImmediately();
  }
  DCHECK(!needs_layout());

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.Open", value);
      })));

  // Animation specification for bottom shelf:
  //
  // Y Position: Down 8px → End position (visually moves up)
  // Duration: 150ms
  // Ease: (0.00, 0.00, 0.20, 1.00)
  //
  // Height: 75% → 100%
  // Duration: 150ms
  // Ease: (0.00, 0.00, 0.20, 1.00)
  //
  // Opacity: 0% → 100%
  // Duration: 150ms
  // Ease: Linear
  //
  // Side shelf uses shorter duration (100ms) and visually moves down.

  // Start by making the layer shorter, pushed down, and transparent.
  const gfx::Rect target_bounds = layer()->GetTargetBounds();
  layer()->SetBounds(GetShowHideAnimationBounds(is_side_shelf, target_bounds));
  layer()->SetOpacity(0.f);

  const base::TimeDelta duration =
      is_side_shelf ? base::Milliseconds(100) : base::Milliseconds(150);

  // Animate the layer to fully opaque at its target bounds.
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&AppListBubbleView::OnShowAnimationEnded,
                              weak_factory_.GetWeakPtr(), target_bounds))
      .OnAborted(base::BindOnce(&AppListBubbleView::OnShowAnimationEnded,
                                weak_factory_.GetWeakPtr(), target_bounds))
      .Once()
      .SetDuration(duration)
      .SetBounds(layer(), target_bounds, gfx::Tween::LINEAR_OUT_SLOW_IN)
      .SetOpacity(layer(), 1.f, gfx::Tween::LINEAR);

  // AppListBubbleAppsPage handles moving the individual views. It also handles
  // smoothness reporting, because the view movement animation has a longer
  // duration.
  if (current_page_ == AppListBubblePage::kApps)
    apps_page_->AnimateShowLauncher(is_side_shelf);

  // Note: The assistant page handles its own show animation internally.
}

void AppListBubbleView::StartHideAnimation(
    bool is_side_shelf,
    base::OnceClosure on_animation_ended) {
  is_hiding_ = true;
  on_hide_animation_ended_ = std::move(on_animation_ended);

  // For performance, don't animate the shadow.
  view_shadow_.reset();

  // Ensure any in-progress animations have their cleanup callbacks called.
  AbortAllAnimations();

  if (current_page_ == AppListBubblePage::kApps)
    apps_page_->PrepareForHideLauncher();

  const gfx::Rect target_bounds = layer()->GetTargetBounds();

  if (view_delegate_->ShouldDismissImmediately()) {
    // Don't animate, just clean up.
    OnHideAnimationEnded(target_bounds);
    return;
  }

  ui::AnimationThroughputReporter reporter(
      layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int value) {
        base::UmaHistogramPercentage(
            "Apps.ClamshellLauncher.AnimationSmoothness.Close", value);
      })));

  // Animation spec:
  //
  // Y Position: Current → Down 8px, or for side shelf: Current → Up 8px
  // Duration: 100ms
  // Ease: (0.4, 0, 1, 1).
  //
  // Height: 100% → 75%
  // Duration: 100ms
  // Ease: (0.4, 0, 1, 1)
  //
  // Opacity: 100% → 0%
  // Duration: 100ms
  // Ease: Linear
  const gfx::Rect final_bounds =
      GetShowHideAnimationBounds(is_side_shelf, target_bounds);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(&AppListBubbleView::OnHideAnimationEnded,
                              weak_factory_.GetWeakPtr(), target_bounds))
      .OnAborted(base::BindOnce(&AppListBubbleView::OnHideAnimationEnded,
                                weak_factory_.GetWeakPtr(), target_bounds))
      .Once()
      .SetDuration(base::Milliseconds(100))
      .SetBounds(layer(), final_bounds, gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetOpacity(layer(), 0.f, gfx::Tween::LINEAR);
}

void AppListBubbleView::AbortAllAnimations() {
  apps_page_->AbortAllAnimations();
  search_page_->AbortAllAnimations();
  apps_collections_page_->AbortAllAnimations();
  layer()->GetAnimator()->AbortAllAnimations();
}

bool AppListBubbleView::Back() {
  if (showing_folder_) {
    folder_view_->CloseFolderPage();
    return true;
  }
  if (search_box_view_->HasSearch()) {
    // When showing the `AppListBubblePage::kAssistant`, it will not change the
    // search box text. Therefore, if the `AppListBubblePage::kAssistant` is
    // from search result, the search query is not empty, Back() here will clear
    // the search, QueryChanged() will set the page to
    // `AppListBubblePage::kApps`. If the `AppListBubblePage::kAssistant` is
    // from other `AssistantVisibilityEntryPoint`, the search box is empty,
    // Back() will return false and then the AppList will be closed.
    if (IsShowingEmbeddedAssistantUI()) {
      view_delegate_->EndAssistant(
          assistant::AssistantExitPoint::kBackInLauncher);
    }
    search_box_view_->ClearSearch();
    return true;
  }

  return false;
}

void AppListBubbleView::ShowPage(AppListBubblePage page) {
  DVLOG(1) << __PRETTY_FUNCTION__ << " page " << page;
  if (page == current_page_) {
    return;
  }

  const AppListBubblePage previous_page = current_page_;
  current_page_ = page;

  // The assistant has its own text input field.
  search_box_view_->SetVisible(page != AppListBubblePage::kAssistant);
  separator_->SetVisible(page != AppListBubblePage::kAssistant);

  const bool supports_anchored_dialogs =
      page == AppListBubblePage::kApps ||
      page == AppListBubblePage::kAppsCollections ||
      page == AppListBubblePage::kSearch;

  search_page_dialog_controller_->Reset(/*enabled=*/supports_anchored_dialogs);
  assistant_page_->SetVisible(page == AppListBubblePage::kAssistant);
  switch (current_page_) {
    case AppListBubblePage::kNone:
      NOTREACHED();
    case AppListBubblePage::kApps:
      apps_page_->ResetScrollPosition();
      if (previous_page == AppListBubblePage::kSearch) {
        // Trigger hiding first so animations don't overlap.
        search_page_->AnimateHidePage();
        apps_page_->AnimateShowPage();
      } else if (previous_page == AppListBubblePage::kAppsCollections) {
        apps_collections_page_->AnimateHidePage();
        apps_page_->AnimateShowPage();
      } else {
        apps_page_->SetVisible(true);
        apps_collections_page_->SetVisible(false);
        search_page_->SetVisible(false);
      }
      a11y_announcer_->AnnounceAppListShown();
      MaybeFocusAndActivateSearchBox();
      break;
    case AppListBubblePage::kAppsCollections:
      if (previous_page == AppListBubblePage::kApps) {
        apps_page_->AnimateHidePage();
        apps_collections_page_->AnimateShowPage();
      } else if (previous_page == AppListBubblePage::kSearch) {
        // Trigger hiding first so animations don't overlap.
        search_page_->AnimateHidePage();
        apps_collections_page_->AnimateShowPage();
      } else {
        search_page_->SetVisible(false);
        apps_page_->SetVisible(false);
        apps_collections_page_->SetVisible(true);
      }
      MaybeFocusAndActivateSearchBox();
      break;
    case AppListBubblePage::kSearch:
      if (previous_page == AppListBubblePage::kApps) {
        apps_page_->AnimateHidePage();
        search_page_->AnimateShowPage();
      } else if (previous_page == AppListBubblePage::kAppsCollections) {
        apps_collections_page_->AnimateHidePage();
        search_page_->AnimateShowPage();
      } else {
        apps_page_->SetVisible(false);
        apps_collections_page_->SetVisible(false);
        search_page_->SetVisible(true);
      }
      MaybeFocusAndActivateSearchBox();
      break;
    case AppListBubblePage::kAssistant:
      if (showing_folder_)
        HideFolderView(/*animate=*/false, /*hide_for_reparent=*/false);
      if (previous_page == AppListBubblePage::kApps)
        apps_page_->AnimateHidePage();
      else
        apps_page_->SetVisible(false);
      search_page_->SetVisible(false);
      apps_collections_page_->SetVisible(false);
      // Explicitly set search box inactive so the next attempt to activate it
      // will succeed.
      search_box_view_->SetSearchBoxActive(
          false,
          /*event_type=*/ui::EventType::kUnknown);
      assistant_page_->RequestFocus();
      break;
  }
}

bool AppListBubbleView::IsShowingEmbeddedAssistantUI() const {
  return current_page_ == AppListBubblePage::kAssistant;
}

void AppListBubbleView::ShowEmbeddedAssistantUI() {
  DVLOG(1) << __PRETTY_FUNCTION__;
  if (IsShowingEmbeddedAssistantUI()) {
    return;
  }
  ShowPage(AppListBubblePage::kAssistant);
}

int AppListBubbleView::GetHeightToFitAllApps() const {
  return apps_page_->scroll_view()->contents()->bounds().height() +
         search_box_view_->GetPreferredSize().height();
}

void AppListBubbleView::UpdateContinueSectionVisibility() {
  apps_page_->UpdateContinueSectionVisibility();
}

void AppListBubbleView::UpdateForNewSortingOrder(
    const std::optional<AppListSortOrder>& new_order,
    bool animate,
    base::OnceClosure update_position_closure) {
  // If app list sort order change is animated, hide any open folders as part of
  // animation. If the update is not animated, e.g. when committing sort order,
  // keep the folder open to prevent folder closure when apps within the folder
  // are reordered, or whe the folder gets renamed.
  if (animate && showing_folder_)
    HideFolderView(/*animate=*/false, /*hide_for_reparent=*/false);

  base::OnceClosure done_closure;
  if (animate) {
    // The search box to ignore a11y events during the reorder animation
    // so that the announcement of app list reorder is made before that of
    // focus change.
    SetViewIgnoredForAccessibility(search_box_view_, true);

    // Focus on the search box before starting the reorder animation to prevent
    // focus moving through app list items as they're being hidden for order
    // update animation.
    search_box_view_->search_box()->RequestFocus();

    done_closure =
        base::BindOnce(&AppListBubbleView::OnAppListReorderAnimationDone,
                       weak_factory_.GetWeakPtr());
  }

  apps_page_->UpdateForNewSortingOrder(new_order, animate,
                                       std::move(update_position_closure),
                                       std::move(done_closure));
}

bool AppListBubbleView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
    case ui::VKEY_BROWSER_BACK:
      // If the ContentsView does not handle the back action, then this is the
      // top level, so we close the app list.
      if (!Back()) {
        view_delegate_->DismissAppList();
      }
      break;
    default:
      NOTREACHED();
  }

  // Don't let the accelerator propagate any further.
  return true;
}

void AppListBubbleView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The folder view has custom layout code that centers the folder over the
  // associated root apps grid folder item.
  // Folder bounds depend on the associated item view location in the apps
  // grid, so the folder needs to be laid out after the root apps grid.
  if (showing_folder_) {
    gfx::Rect folder_bounding_box = GetLocalBounds();
    folder_bounding_box.Inset(kFolderViewInset);
    folder_view_->SetBoundingBox(folder_bounding_box);
    folder_view_->UpdatePreferredBounds();
    // NOTE: Folder view bounds are also modified during reparent drag when the
    // view is "visible" but hidden offscreen. See app_list_folder_view.cc.
    folder_view_->SetBoundsRect(folder_view_->preferred_bounds());
    // The folder view updates the shadow bounds on its own when animating, so
    // only update the shadow bounds here when not animating.
    if (!folder_view_->IsAnimationRunning()) {
      folder_view_->UpdateShadowBounds();
    }
  }
}

void AppListBubbleView::QueryChanged(const std::u16string& trimmed_query,
                                     bool initiated_by_user) {
  if (current_page_ != AppListBubblePage::kNone) {
    search_page_->search_view()->UpdateForNewSearch(!trimmed_query.empty());
    if (!trimmed_query.empty()) {
      ShowPage(AppListBubblePage::kSearch);
    } else if (AppsCollectionsController::Get()->ShouldShowAppsCollection()) {
      ShowPage(AppListBubblePage::kAppsCollections);
    } else {
      ShowPage(AppListBubblePage::kApps);
    }
  }
  SchedulePaint();
}

void AppListBubbleView::AssistantButtonPressed() {
  // Showing the assistant via the delegate triggers the assistant's visibility
  // change notification and ensures its initial visual state is correct.
  view_delegate_->StartAssistant(
      assistant::AssistantEntryPoint::kLauncherSearchBoxIcon);
}

void AppListBubbleView::CloseButtonPressed() {
  // Activate and focus the search box.
  search_box_view_->SetSearchBoxActive(true,
                                       /*event_type=*/ui::EventType::kUnknown);
  search_box_view_->ClearSearch();
}

void AppListBubbleView::OnSearchBoxKeyEvent(ui::KeyEvent* event) {
  // Nothing to do. Search box starts focused, and FocusManager handles arrow
  // key traversal from there. ButtonFocusSkipper above handles skipping the
  // assistant and hide continue section buttons on arrow up and arrow down.
}

bool AppListBubbleView::CanSelectSearchResults() {
  return current_page_ == AppListBubblePage::kSearch &&
         search_page_->search_view()->CanSelectSearchResults();
}

void AppListBubbleView::ShowFolderForItemView(AppListItemView* folder_item_view,
                                              bool focus_name_input,
                                              base::OnceClosure hide_callback) {
  DVLOG(1) << __FUNCTION__;
  if (folder_view_->IsAnimationRunning()) {
    return;
  }

  // TODO(jamescook): Record metric for folder open. Either use the existing
  // Apps.AppListFolderOpened or introduce a new metric.

  DCHECK(folder_item_view->is_folder());
  folder_view_->ConfigureForFolderItemView(folder_item_view,
                                           std::move(hide_callback));
  showing_folder_ = true;
  DeprecatedLayoutImmediately();
  folder_background_view_->SetVisible(true);
  folder_view_->ScheduleShowHideAnimation(/*show=*/true,
                                          /*hide_for_reparent=*/false);
  if (focus_name_input) {
    folder_view_->FocusNameInput();
  } else if (apps_page_->scrollable_apps_grid_view()->has_selected_view() ||
             IsSpokenFeedbackEnabled()) {
    // If the user is keyboard navigating, or using ChromeVox (spoken feedback),
    // move focus into the folder.
    folder_view_->FocusFirstItem(/*silently=*/false);
  } else {
    // Release focus so that disabling the views below does not shift focus
    // into the folder grid.
    GetFocusManager()->ClearFocus();
  }
  // Disable items behind the folder so they will not be reached in focus
  // traversal.
  DisableFocusForShowingActiveFolder(true);
}

void AppListBubbleView::ShowApps(AppListItemView* folder_item_view,
                                 bool select_folder) {
  DVLOG(1) << __FUNCTION__;
  if (folder_view_->IsAnimationRunning()) {
    return;
  }

  HideFolderView(/*animate=*/folder_item_view, /*hide_for_reparent=*/false);

  if (folder_item_view && select_folder)
    folder_item_view->RequestFocus();
  else
    search_box_view_->search_box()->RequestFocus();
}

void AppListBubbleView::ReparentFolderItemTransit(
    AppListFolderItem* folder_item) {
  DVLOG(1) << __FUNCTION__;
  if (folder_view_->IsAnimationRunning()) {
    return;
  }

  HideFolderView(/*animate=*/true, /*hide_for_reparent=*/true);
}

void AppListBubbleView::ReparentDragEnded() {
  DVLOG(1) << __FUNCTION__;
  // Nothing to do.
}

void AppListBubbleView::InitializeUIForBubbleView() {
  assistant_page_->InitializeUIForBubbleView();
}

void AppListBubbleView::DisableFocusForShowingActiveFolder(bool disabled) {
  search_box_view_->SetEnabled(!disabled);
  SetViewIgnoredForAccessibility(search_box_view_, disabled);

  apps_page_->DisableFocusForShowingActiveFolder(disabled);
}

void AppListBubbleView::OnShowAnimationEnded(const gfx::Rect& layer_bounds) {
  // Restore the layer bounds. If the animation completed normally, this isn't
  // visible because the bounds won't change. If the animation was aborted, this
  // is needed to reset state before starting the hide animation.
  layer()->SetBounds(layer_bounds);

  if (current_page_ == AppListBubblePage::kAppsCollections) {
    apps_collections_page_->RecordAboveTheFoldMetrics();
  } else if (current_page_ == AppListBubblePage::kApps) {
    apps_page_->RecordAboveTheFoldMetrics();
  }
}

void AppListBubbleView::OnHideAnimationEnded(const gfx::Rect& layer_bounds) {
  // Restore the layer bounds. This isn't visible because opacity is 0.
  layer()->SetBounds(layer_bounds);

  // NOTE: This may cause a query update and switch to the apps page if the
  // if the search box was not empty.
  search_box_view_->ClearSearch();

  // Hide any open folder.
  HideFolderView(/*animate=*/false, /*hide_for_reparent=*/false);

  // Reset pages to default visibility.
  current_page_ = AppListBubblePage::kNone;
  apps_page_->SetVisible(true);
  search_page_->SetVisible(false);
  assistant_page_->SetVisible(false);

  is_hiding_ = false;
  if (on_hide_animation_ended_) {
    std::move(on_hide_animation_ended_).Run();
  }
}

void AppListBubbleView::HideFolderView(bool animate, bool hide_for_reparent) {
  showing_folder_ = false;
  DeprecatedLayoutImmediately();
  folder_background_view_->SetVisible(false);
  if (!hide_for_reparent) {
    apps_page_->scrollable_apps_grid_view()->ResetForShowApps();
    folder_view_->ResetItemsGridForClose();
  }
  if (animate) {
    folder_view_->ScheduleShowHideAnimation(/*show=*/false, hide_for_reparent);
  } else {
    folder_view_->HideViewImmediately();
  }
  DisableFocusForShowingActiveFolder(false);
}

void AppListBubbleView::OnAppListReorderAnimationDone() {
  // Re-enable the search box to handle a11y events.
  SetViewIgnoredForAccessibility(search_box_view_, false);
}

void AppListBubbleView::MaybeFocusAndActivateSearchBox() {
  // Don't focus the search box while the view is hiding. The app list may be
  // dismissed when focus moves to another view (e.g. the message center).
  // Attempting to focus the search box could make that other view close.
  // https://crbug.com/1313140
  if (is_hiding_) {
    return;
  }

  search_box_view_->SetSearchBoxActive(true,
                                       /*event_type=*/ui::EventType::kUnknown);
  // Explicitly request focus in case the search box was active before.
  search_box_view_->search_box()->RequestFocus();
}

BEGIN_METADATA(AppListBubbleView)
END_METADATA

}  // namespace ash
