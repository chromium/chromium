// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_item.h"

#include "ash/birch/birch_coral_grouped_icon_image.h"
#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_model.h"
#include "ash/birch/coral_util.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/barrier_callback.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

namespace {

constexpr int kCoralIconSize = 14;
constexpr int kCoralAppIconDesiredSize = 64;
constexpr int kCoralMaxSubIconsNum = 4;

constexpr char kMaxDesksToastId[] = "coral_max_desks_toast";

// Callback for the favicon load request in `GetFaviconImageCoral()`. If the
// load fails, passes an empty `ui::ImageModel` to the `barrier_callback`.
void OnGotFaviconImageCoral(
    base::OnceCallback<void(const ui::ImageModel&)> barrier_callback,
    const ui::ImageModel& image) {
  BirchClient* client = Shell::Get()->birch_model()->birch_client();
  if (image.IsImage()) {
    std::move(barrier_callback).Run(std::move(image));
  } else {
    // We need to use this method because the `ui::ImageModel` is constructed
    // from a `gfx::ImageSkia` and not a vector icon.
    std::move(barrier_callback).Run(client->GetChromeBackupIcon());
  }
}

// Callback for the app icon load request in `GetAppIconCoral()`. If the
// load fails, passes an empty `ui::ImageModel` to the `barrier_callback`.
void OnGotAppIconCoral(
    base::OnceCallback<void(const ui::ImageModel&)> barrier_callback,
    const gfx::ImageSkia& image) {
  if (!image.isNull()) {
    std::move(barrier_callback)
        .Run(std::move(ui::ImageModel::FromImageSkia(image)));
  } else {
    // TODO(zxdan): Define a backup icon for apps.
    std::move(barrier_callback).Run(ui::ImageModel());
  }
}

// Draws the Coral grouped icon image with the loaded icons, and passes the
// final result to `BirchChipButton`.
void OnAllFaviconsRetrievedCoral(
    base::OnceCallback<void(const ui::ImageModel&)> final_callback,
    int extra_number,
    const std::vector<ui::ImageModel>& loaded_icons) {
  std::vector<gfx::ImageSkia> resized_icons;

  for (const auto& loaded_icon : loaded_icons) {
    // TODO(zxdan): Once all favicons have backup icons, change this to CHECK.
    if (!loaded_icon.IsEmpty()) {
      // Only a `ui::ImageModel` constructed from a `gfx::ImageSkia` produces a
      // valid result from `GetImage()`. Vector icons will not work.
      resized_icons.emplace_back(gfx::ImageSkiaOperations::CreateResizedImage(
          loaded_icon.GetImage().AsImageSkia(),
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kCoralIconSize, kCoralIconSize)));
    }
  }

  ui::ImageModel composed_image =
      CoralGroupedIconImage::DrawCoralGroupedIconImage(
          /*icons_images=*/resized_icons, extra_number);

  std::move(final_callback).Run(std::move(composed_image));
}

}  // namespace

BirchCoralItem::BirchCoralItem(const std::u16string& coral_title,
                               const std::u16string& coral_text,
                               CoralSource source,
                               const base::Token& group_id)
    : BirchItem(coral_title, coral_text),
      source_(source),
      group_id_(group_id) {}

BirchCoralItem::BirchCoralItem(BirchCoralItem&&) = default;

BirchCoralItem::BirchCoralItem(const BirchCoralItem&) = default;

BirchCoralItem& BirchCoralItem::operator=(const BirchCoralItem&) = default;

bool BirchCoralItem::operator==(const BirchCoralItem& rhs) const = default;

BirchCoralItem::~BirchCoralItem() = default;

BirchItemType BirchCoralItem::GetType() const {
  return BirchItemType::kCoral;
}

std::string BirchCoralItem::ToString() const {
  auto root = base::Value::Dict().Set(
      "Coral item",
      base::Value::Dict().Set("Title", title()).Set("Subtitle", subtitle()));
  return root.DebugString();
}

base::Value::Dict BirchCoralItem::ToCoralItemDetails() const {
  const coral::mojom::GroupPtr& group =
      BirchCoralProvider::Get()->GetGroupById(group_id_);
  base::Value::List items;
  for (const auto& entity : group->entities) {
    if (entity->is_tab()) {
      items.Append(base::Value::Dict()
                       .Set("type", "tab")
                       .Set("title", entity->get_tab()->title)
                       .Set("url", entity->get_tab()->url.spec()));
    } else {
      items.Append(base::Value::Dict()
                       .Set("type", "app")
                       .Set("title", entity->get_app()->title)
                       .Set("id", entity->get_app()->id));
    }
  }
  return std::move(
      base::Value::Dict().Set("title", title()).Set("items", std::move(items)));
}

void BirchCoralItem::PerformAction() {
  // Record basic metrics.
  RecordActionMetrics();

  switch (source_) {
    case CoralSource::kPostLogin: {
      coral::mojom::GroupPtr group =
          BirchCoralProvider::Get()->ExtractGroupById(group_id_);
      Shell::Get()->coral_delegate()->LaunchPostLoginGroup(std::move(group));
      BirchCoralProvider::Get()->OnPostLoginClusterRestored();
      base::UmaHistogramEnumeration("Ash.Birch.Coral.Action",
                                    ActionType::kRestore);
      // End the Overview after restore.
      // TODO(zxdan|sammie): Consider the restoring failed cases.
      OverviewController::Get()->EndOverview(OverviewEndAction::kCoral,
                                             OverviewEnterExitType::kNormal);
      break;
    }
    case CoralSource::kInSession: {
      if (!DesksController::Get()->CanCreateDesks()) {
        ToastData toast(
            kMaxDesksToastId, ToastCatalogName::kVirtualDesksLimitMax,
            l10n_util::GetStringUTF16(IDS_ASH_DESKS_MAX_NUM_REACHED),
            ToastData::kDefaultToastDuration,
            /*visible_on_lock_screen=*/false);
        Shell::Get()->toast_manager()->Show(std::move(toast));
        return;
      }
      coral::mojom::GroupPtr group =
          BirchCoralProvider::Get()->ExtractGroupById(group_id_);
      Shell::Get()->coral_controller()->OpenNewDeskWithGroup(std::move(group));
      base::UmaHistogramEnumeration("Ash.Birch.Coral.Action",
                                    ActionType::kLaunchToNewDesk);
      break;
    }
    case CoralSource::kUnknown:
      NOTREACHED() << "Invalid response with unknown source.";
  }
}

// TODO(b/362530155): Consider refactoring icon loading logic into
// `CoralGroupedIconImage`.
void BirchCoralItem::LoadIcon(LoadIconCallback original_callback) const {
  const coral::mojom::GroupPtr& group =
      BirchCoralProvider::Get()->GetGroupById(group_id_);

  const coral_util::TabsAndApps tabs_apps =
      coral_util::SplitContentData(group->entities);

  const int page_num = tabs_apps.tabs.size();
  const int app_num = tabs_apps.apps.size();
  const int total_count = page_num + app_num;

  // If the total number of pages and apps exceeds the limit of number of sub
  // icons, only show 3 icons and one extra number label. Otherwise, show all
  // the icons.
  const int icon_requests =
      total_count > kCoralMaxSubIconsNum ? 3 : total_count;

  // Barrier callback that collects the results of multiple favicon loads and
  // runs the original load icon callback.
  const auto barrier_callback = base::BarrierCallback<const ui::ImageModel&>(
      /*num_callbacks=*/icon_requests,
      /*done_callback=*/base::BindOnce(
          OnAllFaviconsRetrievedCoral,
          base::BindOnce(std::move(original_callback),
                         PrimaryIconType::kCoralGroupIcon,
                         SecondaryIconType::kNoIcon),
          /*extra_number=*/total_count > icon_requests
              ? total_count - icon_requests
              : 0));

  for (int i = 0; i < std::min(icon_requests, page_num); i++) {
    // For each `url`, retrieve the icon using favicon service, and run the
    // `barrier_callback` with the image result.
    GetFaviconImageCoral(tabs_apps.tabs[i].url, barrier_callback);
  }

  for (int i = 0; i < icon_requests - page_num; i++) {
    // For each `id`, retrieve the icon using `saved_desk_delegate`, and run the
    // `barrier_callback` with the image result.
    GetAppIconCoral(tabs_apps.apps[i].id, barrier_callback);
  }
}

BirchAddonType BirchCoralItem::GetAddonType() const {
  return BirchAddonType::kCoralButton;
}

std::u16string BirchCoralItem::GetAddonAccessibleName() const {
  // The add on tooltip and a11y name is determined by the presence of the
  // selection UI. It will be handled in `BirchChipButton`.
  return u"Placeholder";
}

void BirchCoralItem::GetFaviconImageCoral(
    const GURL& url,
    base::OnceCallback<void(const ui::ImageModel&)> barrier_callback) const {
  BirchClient* client = Shell::Get()->birch_model()->birch_client();
  client->GetFaviconImage(
      url, /*is_page_url=*/true,
      base::BindOnce(OnGotFaviconImageCoral, std::move(barrier_callback)));
}

void BirchCoralItem::GetAppIconCoral(
    const std::string& app_id,
    base::OnceCallback<void(const ui::ImageModel&)> barrier_callback) const {
  Shell::Get()->saved_desk_delegate()->GetIconForAppId(
      app_id, kCoralAppIconDesiredSize,
      base::BindOnce(OnGotAppIconCoral, std::move(barrier_callback)));
}

}  // namespace ash
