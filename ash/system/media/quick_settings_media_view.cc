// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/quick_settings_media_view.h"

#include "ash/public/cpp/pagination/pagination_controller.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pagination_view.h"
#include "ash/system/media/quick_settings_media_view_controller.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// The dimensions of the overall media view.
constexpr int kMediaViewWidth = 400;
constexpr int kMediaViewHeight = 150;

// The y-position of the floating pagination dots view in the media view.
constexpr int kPaginationViewHeight = 123;

// A scroll view that arranges all the media items in a row and sets which item
// is shown on the screen by setting the x position of the scroll content.
class MediaScrollView : public views::ScrollView,
                        public PaginationModelObserver {
 public:
  explicit MediaScrollView(PaginationModel* model)
      : views::ScrollView(ScrollView::ScrollWithLayers::kEnabled),
        model_(model) {
    observer_.Observe(model_);
    SetContents(std::make_unique<views::BoxLayoutView>());

    // Remove the default background color.
    SetBackgroundColor(absl::nullopt);

    // The scroll view does not accept any scroll event.
    SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
    SetVerticalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
  }

  MediaScrollView(const MediaScrollView&) = delete;
  MediaScrollView& operator=(const MediaScrollView&) = delete;
  ~MediaScrollView() override = default;

  // views::ScrollView:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(kMediaViewWidth, kMediaViewHeight);
  }

  void Layout() override {
    contents()->SizeToPreferredSize();
    views::ScrollView::Layout();
  }

  // PaginationModelObserver:
  void SelectedPageChanged(int old_selected, int new_selected) override {
    if (model_->is_valid_page(new_selected)) {
      ScrollToOffset(gfx::PointF(new_selected * kMediaViewWidth, 0));
    }
  }

  void TransitionChanged() override {
    // Update the scroll content to show the transition progress of changing
    // from one media item to another.
    const int origin = model_->selected_page() * kMediaViewWidth;
    const int target = model_->transition().target_page * kMediaViewWidth;
    const double progress = model_->transition().progress;
    ScrollToOffset(
        gfx::PointF(gfx::Tween::IntValueBetween(progress, origin, target), 0));
  }

 private:
  raw_ptr<PaginationModel> model_ = nullptr;
  base::ScopedObservation<PaginationModel, PaginationModelObserver> observer_{
      this};
};

}  // namespace

QuickSettingsMediaView::QuickSettingsMediaView(
    QuickSettingsMediaViewController* controller)
    : controller_(controller) {
  // All the views need to paint to layer so that the pagination view can be
  // placed floating above the media scroll view.
  media_scroll_view_ =
      AddChildView(std::make_unique<MediaScrollView>(&pagination_model_));

  pagination_view_ =
      AddChildView(std::make_unique<PaginationView>(&pagination_model_));
  pagination_view_->SetPaintToLayer();
  pagination_view_->layer()->SetFillsBoundsOpaquely(false);

  pagination_controller_ = std::make_unique<PaginationController>(
      &pagination_model_, PaginationController::SCROLL_AXIS_HORIZONTAL,
      base::BindRepeating([](ui::EventType) {}));

  SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_MEDIA_CONTROLS_SUB_MENU_ACCESSIBLE_DESCRIPTION));
}

QuickSettingsMediaView::~QuickSettingsMediaView() = default;

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

gfx::Size QuickSettingsMediaView::CalculatePreferredSize() const {
  return gfx::Size(kMediaViewWidth, kMediaViewHeight);
}

void QuickSettingsMediaView::Layout() {
  media_scroll_view_->SetBounds(0, 0, kMediaViewWidth, kMediaViewHeight);

  // Place the pagination dots view on top of the media view.
  gfx::Size pagination_view_size = pagination_view_->CalculatePreferredSize();
  pagination_view_->SetBounds(
      (kMediaViewWidth - pagination_view_size.width()) / 2,
      kPaginationViewHeight, pagination_view_size.width(),
      pagination_view_size.height());

  // The pagination dots view only shows if there are multiple media items.
  pagination_view_->SetVisible(items_.size() > 1);
}

void QuickSettingsMediaView::OnGestureEvent(ui::GestureEvent* event) {
  // The pagination controller will handle the touch gesture event to swipe
  // between media items.
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds())) {
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    // A tap gesture is handled in the same way as a mouse click event. The
    // controller does not need to know the item id for now so we do not need to
    // record it.
    controller_->OnMediaItemUIClicked(/*id=*/"");
    event->SetHandled();
  }
}

///////////////////////////////////////////////////////////////////////////////
// QuickSettingsMediaView implementations:

void QuickSettingsMediaView::ShowItem(
    const std::string& id,
    std::unique_ptr<global_media_controls::MediaItemUIView> item) {
  DCHECK(!base::Contains(items_, id));
  item->SetPreferredSize(gfx::Size(kMediaViewWidth, kMediaViewHeight));
  items_[id] = media_scroll_view_->contents()->AddChildView(std::move(item));

  pagination_model_.SetTotalPages(items_.size());
  PreferredSizeChanged();
  controller_->SetShowMediaView(true);
}

void QuickSettingsMediaView::HideItem(const std::string& id) {
  if (!base::Contains(items_, id)) {
    return;
  }
  media_scroll_view_->contents()->RemoveChildViewT(items_[id]);
  items_.erase(id);

  pagination_model_.SetTotalPages(items_.size());
  PreferredSizeChanged();
  controller_->SetShowMediaView(!items_.empty());
}

void QuickSettingsMediaView::UpdateItemOrder(std::list<std::string> ids) {
  if (ids.empty()) {
    return;
  }

  // Remove all the media views for re-ordering.
  std::map<const std::string,
           std::unique_ptr<global_media_controls::MediaItemUIView>>
      media_items;
  for (auto& item : items_) {
    media_items[item.first] =
        media_scroll_view_->contents()->RemoveChildViewT(item.second);
  }

  // Add back the media views given the new order.
  for (auto& id : ids) {
    DCHECK(base::Contains(media_items, id));
    items_[id] = media_scroll_view_->contents()->AddChildView(
        std::move(media_items[id]));
  }
}

}  // namespace ash
