// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_item.h"

#include "ash/birch/birch_coral_grouped_icon_image.h"
#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_model.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/barrier_callback.h"
#include "base/json/json_writer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {

namespace {

constexpr int kCoralIconSize = 14;
constexpr int kCoralAppIconDesiredSize = 64;
constexpr int kCoralMaxSubIconsNum = 4;

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
    base::OnceCallback<void(const ui::ImageModel&, SecondaryIconType)>
        final_callback,
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

  std::move(final_callback)
      .Run(std::move(composed_image), SecondaryIconType::kNoIcon);
}

}  // namespace

BirchCoralItem::BirchCoralItem(const std::u16string& coral_title,
                               const std::u16string& coral_text,
                               const std::vector<GURL>& page_urls,
                               const std::vector<std::string>& app_ids,
                               int cluster_id)
    : BirchItem(coral_title, coral_text),
      page_urls_(page_urls),
      app_ids_(app_ids),
      cluster_id_(cluster_id) {
  set_addon_label(u"Show");
}

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
  return base::WriteJson(root).value_or(std::string());
}

void BirchCoralItem::PerformAction(bool is_post_login) {
  coral::mojom::GroupPtr temp_group = coral::mojom::Group::New();
  temp_group->title = "Coral desk";
  for (const GURL& gurl : page_urls_) {
    temp_group->entities.push_back(coral::mojom::EntityKey::NewTabUrl(gurl));
  }
  for (const std::string& app_id : app_ids_) {
    temp_group->entities.push_back(coral::mojom::EntityKey::NewAppId(app_id));
  }

  // Pick first half of the tabs from request for testing.
  const auto& request_items =
      BirchCoralProvider::Get()->GetCoralRequestForTest().content();
  std::vector<coral::mojom::TabPtr> tabs;
  for (const auto& item : request_items) {
    if (item->is_tab()) {
      tabs.push_back(std::move(item->get_tab()));
    }
  }

  for (size_t i = 0; i < tabs.size() / 2; i++) {
    temp_group->entities.push_back(
        coral::mojom::EntityKey::NewTabUrl(tabs[i]->url));
  }

  // TODO(http://b/365839465): Handle post-login case.
  if (is_post_login) {
    Shell::Get()->coral_delegate()->LaunchPostLoginGroup(std::move(temp_group));
    return;
  }

  Shell::Get()->coral_controller()->OpenNewDeskWithGroup(std::move(temp_group));
}

// TODO(b/362530155): Consider refactoring icon loading logic into
// `CoralGroupedIconImage`.
void BirchCoralItem::LoadIcon(LoadIconCallback original_callback) const {
  const int page_num = page_urls_.size();
  const int app_num = app_ids_.size();
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
          OnAllFaviconsRetrievedCoral, std::move(original_callback),
          /*extra_number=*/total_count > icon_requests
              ? total_count - icon_requests
              : 0));

  for (int i = 0; i < std::min(icon_requests, page_num); i++) {
    // For each `url`, retrieve the icon using favicon service, and run the
    // `barrier_callback` with the image result.
    GetFaviconImageCoral(page_urls_[i], barrier_callback);
  }

  for (int i = 0; i < icon_requests - page_num; i++) {
    // For each `id`, retrieve the icon using `saved_desk_delegate`, and run the
    // `barrier_callback` with the image result.
    GetAppIconCoral(app_ids_[i], barrier_callback);
  }
}

BirchAddonType BirchCoralItem::GetAddonType() const {
  return BirchAddonType::kCoralButton;
}

std::u16string BirchCoralItem::GetAddonAccessibleName() const {
  return u"Show";
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
