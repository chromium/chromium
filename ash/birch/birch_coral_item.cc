// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/birch/birch_coral_item.h"

#include "ash/birch/birch_coral_grouped_icon_image.h"
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
    std::move(barrier_callback).Run(ui::ImageModel());
  }
}

// Draws the Coral grouped icon image with the loaded icons, and passes the
// final result to `BirchChipButton`.
void OnAllFaviconsRetrievedCoral(
    base::OnceCallback<void(const ui::ImageModel&, SecondaryIconType)>
        final_callback,
    const std::vector<ui::ImageModel>& loaded_icons) {
  std::vector<gfx::ImageSkia> resized_icons;

  for (const auto& loaded_icon : loaded_icons) {
    if (!loaded_icon.IsEmpty()) {
      // Only a `ui::ImageModel` constructed from a `gfx::ImageSkia` produces a
      // valid result from `GetImage()`. Vector icons will not work.
      resized_icons.emplace_back(gfx::ImageSkiaOperations::CreateResizedImage(
          loaded_icon.GetImage().AsImageSkia(),
          skia::ImageOperations::RESIZE_BEST,
          gfx::Size(kCoralIconSize, kCoralIconSize)));
    }
  }

  // TODO(owenzhang): Hook up correct extra_number calculation.
  ui::ImageModel composed_image =
      CoralGroupedIconImage::DrawCoralGroupedIconImage(
          /*icons_images=*/resized_icons, /*extra_tabs_number=*/7);

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
  // TODO(sammiequon): Remove hardcoded group.
  coral::mojom::GroupPtr temp_group = coral::mojom::Group::New();
  temp_group->title = "Coral desk";
  temp_group->entities.push_back(
      coral::mojom::EntityKey::NewTabUrl(GURL("https://www.ikea.com/")));
  temp_group->entities.push_back(
      coral::mojom::EntityKey::NewTabUrl(GURL("https://www.nhl.com/")));

  if (is_post_login) {
    Shell::Get()->coral_delegate()->LaunchPostLoginGroup(std::move(temp_group));
    return;
  }

  DesksController* desks_controller = DesksController::Get();
  if (!desks_controller->CanCreateDesks()) {
    return;
  }

  desks_controller->NewDesk(DesksCreationRemovalSource::kCoral,
                            base::UTF8ToUTF16(temp_group->title));
  desks_controller->ActivateDesk(desks_controller->desks().back().get(),
                                 DesksSwitchSource::kCoral);
  Shell::Get()->coral_delegate()->OpenNewDeskWithGroup(std::move(temp_group));
}

// TODO(b/362530155): Consider refactoring icon loading logic into
// `CoralGroupedIconImage`.
void BirchCoralItem::LoadIcon(LoadIconCallback original_callback) const {
  // Barrier callback that collects the results of multiple favicon loads and
  // runs the original load_icon callback.
  const auto barrier_callback = base::BarrierCallback<const ui::ImageModel&>(
      /*num_callbacks=*/page_urls_.size() + app_ids_.size(),
      /*done_callback=*/base::BindOnce(OnAllFaviconsRetrievedCoral,
                                       std::move(original_callback)));

  for (const auto& url : page_urls_) {
    // For each `url`, retrieve the icon using favicon_service, and run the
    // `barrier_callback` with the image result.
    GetFaviconImageCoral(url, barrier_callback);
  }

  for (const auto& id : app_ids_) {
    // For each `id`, retrieve the icon using `saved_desk_delegate`, and run the
    // `barrier_callback` with the image result.
    GetAppIconCoral(id, barrier_callback);
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
