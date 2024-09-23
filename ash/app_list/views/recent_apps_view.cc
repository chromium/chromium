// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/recent_apps_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/app_list_item_view_grid_delegate.h"
#include "ash/app_list/views/app_list_keyboard_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

namespace ash {
namespace {

constexpr size_t kMinRecommendedApps = 4;
constexpr size_t kMaxRecommendedApps = 5;

// Converts a search result app ID to an app list item ID.
std::string ItemIdFromAppId(const std::string& app_id) {
  // Convert chrome-extension://<id> to just <id>.
  if (base::StartsWith(app_id, extensions::kExtensionScheme)) {
    GURL url(app_id);
    return url.host();
  }
  return app_id;
}

struct RecentAppInfo {
  RecentAppInfo(AppListItem* item, SearchResult* result)
      : item(item), result(result) {}
  RecentAppInfo(const RecentAppInfo&) = default;
  RecentAppInfo& operator=(RecentAppInfo&) = default;
  ~RecentAppInfo() = default;

  raw_ptr<AppListItem> item;
  raw_ptr<SearchResult> result;
};

// Returns a list of recent apps by filtering zero-state suggestion data.
std::vector<RecentAppInfo> GetRecentApps(
    AppListModel* model,
    SearchModel* search_model,
    const std::vector<std::string>& ids_to_ignore) {
  std::vector<RecentAppInfo> recent_apps;

  SearchModel::SearchResults* results = search_model->results();
  for (size_t i = 0; i < results->item_count(); ++i) {
    SearchResult* result = results->GetItemAt(i);
    if (result->display_type() != SearchResultDisplayType::kRecentApps)
      continue;

    std::string item_id = ItemIdFromAppId(result->id());
    if (base::Contains(ids_to_ignore, item_id))
      continue;

    AppListItem* item = model->FindItem(item_id);
    if (!item)
      continue;

    recent_apps.emplace_back(item, result);

    if (recent_apps.size() == kMaxRecommendedApps)
      break;
  }

  return recent_apps;
}

}  // namespace

// The grid delegate for each AppListItemView. Recent app icons cannot be
// dragged, so this implementation is mostly a stub.
class RecentAppsView::GridDelegateImpl : public AppListItemViewGridDelegate {
 public:
  explicit GridDelegateImpl(AppListViewDelegate* view_delegate)
      : view_delegate_(view_delegate) {}
  GridDelegateImpl(const GridDelegateImpl&) = delete;
  GridDelegateImpl& operator=(const GridDelegateImpl&) = delete;
  ~GridDelegateImpl() override = default;

  // AppListItemView::GridDelegate:
  bool IsInFolder() const override { return false; }
  void SetSelectedView(AppListItemView* view) override {
    DCHECK(view);
    if (view == selected_view_)
      return;
    // Ensure the translucent background of the previous selection goes away.
    if (selected_view_)
      selected_view_->SchedulePaint();
    selected_view_ = view;
    // Ensure the translucent background of this selection is painted.
    selected_view_->SchedulePaint();
    selected_view_->NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
  }
  void ClearSelectedView() override { selected_view_ = nullptr; }
  bool IsSelectedView(const AppListItemView* view) const override {
    return view == selected_view_;
  }
  void EndDrag(bool cancel) override {}
  void OnAppListItemViewActivated(AppListItemView* pressed_item_view,
                                  const ui::Event& event) override {
    // NOTE: Avoid using |item->id()| as the parameter. In some rare situations,
    // activating the item may destruct it. Using the reference to an object
    // which may be destroyed during the procedure as the function parameter
    // may bring the crash like https://crbug.com/990282.
    const std::string id = pressed_item_view->item()->id();
    RecordAppListByCollectionLaunched(
        pressed_item_view->item()->collection_id(),
        /*is_apps_collections_page=*/false);

    // `this` may be deleted after activation.
    view_delegate_->ActivateItem(id, event.flags(),
                                 AppListLaunchedFrom::kLaunchedFromRecentApps,
                                 IsAboveTheFold(pressed_item_view));
  }

  bool IsAboveTheFold(AppListItemView* item_view) override {
    // Recent apps are always above the fold.
    return true;
  }

 private:
  const raw_ptr<AppListViewDelegate> view_delegate_;
  raw_ptr<AppListItemView, DanglingUntriaged> selected_view_ = nullptr;
};

RecentAppsView::RecentAppsView(AppListKeyboardController* keyboard_controller,
                               AppListViewDelegate* view_delegate)
    : keyboard_controller_(keyboard_controller),
      view_delegate_(view_delegate),
      grid_delegate_(std::make_unique<GridDelegateImpl>(view_delegate_)) {
  DCHECK(keyboard_controller_);
  DCHECK(view_delegate_);
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout_->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  // TODO(https://crbug.com/1298211): This needs a designated string resource.
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_RECENT_APPS_A11Y_NAME),
      ax::mojom::NameFrom::kAttribute);
  SetVisible(false);
}

RecentAppsView::~RecentAppsView() {
  if (model_)
    model_->RemoveObserver(this);
}

void RecentAppsView::OnAppListItemWillBeDeleted(AppListItem* item) {
  std::vector<std::string> ids_to_remove;

  for (AppListItemView* view : item_views_) {
    if (view->item() && view->item() == item)
      ids_to_remove.push_back(view->item()->id());
  }
  if (!ids_to_remove.empty()) {
    UpdateResults(ids_to_remove);
    UpdateVisibility();
  }
}

void RecentAppsView::UpdateAppListConfig(const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  for (ash::AppListItemView* item_view : item_views_) {
    item_view->UpdateAppListConfig(app_list_config);
  }
}

void RecentAppsView::UpdateResults(
    const std::vector<std::string>& ids_to_ignore) {
  if (!search_model_ || !model_)
    return;

  DCHECK(app_list_config_);
  item_views_.clear();
  RemoveAllChildViews();

  std::vector<RecentAppInfo> apps =
      GetRecentApps(model_, search_model_, ids_to_ignore);
  if (apps.size() < kMinRecommendedApps) {
    if (auto* notifier = view_delegate_->GetNotifier()) {
      notifier->NotifyResultsUpdated(SearchResultDisplayType::kRecentApps, {});
    }
    return;
  }

  if (auto* notifier = view_delegate_->GetNotifier()) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const RecentAppInfo& app : apps)
      notifier_results.emplace_back(
          app.result->id(), app.result->metrics_type(),
          app.result->continue_file_suggestion_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kRecentApps,
                                   notifier_results);
  }

  for (const RecentAppInfo& app : apps) {
    auto* item_view = AddChildView(std::make_unique<AppListItemView>(
        app_list_config_, grid_delegate_.get(), app.item, view_delegate_,
        AppListItemView::Context::kRecentAppsView));
    item_view->UpdateAppListConfig(app_list_config_);
    item_views_.push_back(item_view);
    item_view->InitializeIconLoader();
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                           /*send_native_event=*/true);
}

void RecentAppsView::SetModels(SearchModel* search_model, AppListModel* model) {
  if (model_ != model) {
    if (model_)
      model_->RemoveObserver(this);
    model_ = model;
    if (model_)
      model_->AddObserver(this);
  }

  search_model_ = search_model;
  UpdateResults(/*ids_to_ignore=*/{});
  UpdateVisibility();
}

void RecentAppsView::UpdateVisibility() {
  const bool has_enough_apps = item_views_.size() >= kMinRecommendedApps;
  const bool hidden_by_user = view_delegate_->ShouldHideContinueSection();
  const bool visible = has_enough_apps && !hidden_by_user;
  SetVisible(visible);
  if (auto* notifier = view_delegate_->GetNotifier()) {
    notifier->NotifyContinueSectionVisibilityChanged(
        SearchResultDisplayType::kRecentApps, visible);
  }
}

int RecentAppsView::GetItemViewCount() const {
  return item_views_.size();
}

AppListItemView* RecentAppsView::GetItemViewAt(int index) const {
  if (static_cast<int>(item_views_.size()) <= index)
    return nullptr;
  return item_views_[index];
}

void RecentAppsView::DisableFocusForShowingActiveFolder(bool disabled) {
  for (views::View* child : children())
    child->SetEnabled(!disabled);

  // Prevent items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

bool RecentAppsView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_UP) {
    MoveFocusUp();
    return true;
  }
  if (event.key_code() == ui::VKEY_DOWN) {
    MoveFocusDown();
    return true;
  }
  return false;
}

void RecentAppsView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The AppsGridView's space between items is the sum of the padding on left
  // and on right of the individual tiles. Because of rounding errors, there can
  // be an actual difference of 1px over the actual distribution of space
  // needed, and because this is not compensated on the other columns, the grid
  // carries over the error making it progressively more significant for each
  // column. For the RecentAppsView tiles to match the grid we need to calculate
  // padding as the AppsGridView does to account for the rounding errors and
  // then double it, so it is exactly the same spacing as the AppsGridView.
  layout_->set_between_child_spacing(2 * CalculateTilePadding());
}

void RecentAppsView::MoveFocusUp() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  DCHECK(!children().empty());
  keyboard_controller_->MoveFocusUpFromRecents();
}

void RecentAppsView::MoveFocusDown() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  int column = GetColumnOfFocusedChild();
  DCHECK_GE(column, 0);
  keyboard_controller_->MoveFocusDownFromRecents(column);
}

int RecentAppsView::GetColumnOfFocusedChild() const {
  int column = 0;
  for (views::View* child : children()) {
    if (!views::IsViewClass<AppListItemView>(child))
      continue;
    if (child->HasFocus())
      return column;
    ++column;
  }
  return -1;
}

int RecentAppsView::CalculateTilePadding() const {
  int content_width = GetContentsBounds().width();
  int tile_width = app_list_config_->grid_tile_width();
  int width_to_distribute = content_width - kMaxRecommendedApps * tile_width;

  return width_to_distribute / ((kMaxRecommendedApps - 1) * 2);
}

BEGIN_METADATA(RecentAppsView)
END_METADATA

}  // namespace ash
