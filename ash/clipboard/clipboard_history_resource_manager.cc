// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

constexpr int kPlaceholderImageEdgePadding = 5;
constexpr int kPlaceholderImageWidth = 234;
constexpr int kPlaceholderImageHeight = 74;
constexpr int kPlaceholderImageOutlineCornerRadius = 8;
constexpr int kPlaceholderImageSVGSize = 32;

// Used to draw the UnrenderedHTMLPlaceholderImage, which is shown while HTML is
// rendering. Drawn in order to turn the square and single colored SVG into a
// multicolored rectangle image.
class UnrenderedHTMLPlaceholderImage : public gfx::CanvasImageSource {
 public:
  UnrenderedHTMLPlaceholderImage()
      : gfx::CanvasImageSource(
            gfx::Size(kPlaceholderImageWidth, kPlaceholderImageHeight)) {}
  UnrenderedHTMLPlaceholderImage(const UnrenderedHTMLPlaceholderImage&) =
      delete;
  UnrenderedHTMLPlaceholderImage& operator=(
      const UnrenderedHTMLPlaceholderImage&) = delete;
  ~UnrenderedHTMLPlaceholderImage() override = default;

  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleGrey100);
    canvas->DrawRoundRect(
        {kPlaceholderImageEdgePadding, kPlaceholderImageEdgePadding,
         kPlaceholderImageWidth - 2 * kPlaceholderImageEdgePadding,
         kPlaceholderImageHeight - 2 * kPlaceholderImageEdgePadding},
        kPlaceholderImageOutlineCornerRadius, flags);

    flags = cc::PaintFlags();
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    const gfx::ImageSkia center_image =
        gfx::CreateVectorIcon(kUnrenderedHtmlPlaceholderIcon,
                              kPlaceholderImageSVGSize, gfx::kGoogleGrey600);
    canvas->DrawImageInt(
        center_image, (size().width() - center_image.size().width()) / 2,
        (size().height() - center_image.size().height()) / 2, flags);
  }
};

// Helpers ---------------------------------------------------------------------

// Returns the localized string for the specified |resource_id|.
base::string16 GetLocalizedString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      resource_id);
}

// Returns the label to display for the custom data contained within |data|.
base::string16 GetLabelForCustomData(const ui::ClipboardData& data) {
  // Currently the only supported type of custom data is file system data. This
  // code should not be reached if `data` does not contain file system data.
  base::string16 sources = ClipboardHistoryUtil::GetFileSystemSources(data);
  if (sources.empty()) {
    NOTREACHED();
    return base::string16();
  }

  // Split sources into a list.
  std::vector<base::StringPiece16> source_list =
      base::SplitStringPiece(sources, base::UTF8ToUTF16("\n"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Strip path information, so all that's left are file names.
  for (auto it = source_list.begin(); it != source_list.end(); ++it)
    *it = it->substr(it->find_last_of(base::UTF8ToUTF16("/")) + 1);

  // Join file names, unescaping encoded character sequences for display. This
  // ensures that "My%20File.txt" will display as "My File.txt".
  return base::UTF8ToUTF16(base::UnescapeURLComponent(
      base::UTF16ToUTF8(base::JoinString(source_list, base::UTF8ToUTF16(", "))),
      base::UnescapeRule::SPACES));
}

}  // namespace

// ClipboardHistoryResourceManager ---------------------------------------------

ClipboardHistoryResourceManager::ClipboardHistoryResourceManager(
    const ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history),
      placeholder_image_model_(
          ui::ImageModel::FromImageSkia(gfx::CanvasImageSource::MakeImageSkia<
                                        UnrenderedHTMLPlaceholderImage>())) {
  clipboard_history_->AddObserver(this);
}

ClipboardHistoryResourceManager::~ClipboardHistoryResourceManager() {
  clipboard_history_->RemoveObserver(this);
  if (ClipboardImageModelFactory::Get())
    ClipboardImageModelFactory::Get()->OnShutdown();
}

ui::ImageModel ClipboardHistoryResourceManager::GetImageModel(
    const ClipboardHistoryItem& item) const {
  // Use a cached image model when possible.
  auto cached_image_model = FindCachedImageModelForItem(item);
  if (cached_image_model == cached_image_models_.end() ||
      cached_image_model->image_model.IsEmpty()) {
    return placeholder_image_model_;
  }
  return cached_image_model->image_model;
}

base::string16 ClipboardHistoryResourceManager::GetLabel(
    const ClipboardHistoryItem& item) const {
  const ui::ClipboardData& data = item.data();
  switch (ClipboardHistoryUtil::CalculateMainFormat(data).value()) {
    case ui::ClipboardInternalFormat::kBitmap:
      return GetLocalizedString(IDS_CLIPBOARD_MENU_IMAGE);
    case ui::ClipboardInternalFormat::kText:
      return base::UTF8ToUTF16(data.text());
    case ui::ClipboardInternalFormat::kHtml:
      return base::UTF8ToUTF16(data.markup_data());
    case ui::ClipboardInternalFormat::kSvg:
      return base::UTF8ToUTF16(data.svg_data());
    case ui::ClipboardInternalFormat::kRtf:
      return GetLocalizedString(IDS_CLIPBOARD_MENU_RTF_CONTENT);
    case ui::ClipboardInternalFormat::kBookmark:
      return base::UTF8ToUTF16(data.bookmark_title());
    case ui::ClipboardInternalFormat::kWeb:
      return GetLocalizedString(IDS_CLIPBOARD_MENU_WEB_SMART_PASTE);
    case ui::ClipboardInternalFormat::kCustom:
      return GetLabelForCustomData(data);
  }
}

void ClipboardHistoryResourceManager::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void ClipboardHistoryResourceManager::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

ClipboardHistoryResourceManager::CachedImageModel::CachedImageModel() = default;

ClipboardHistoryResourceManager::CachedImageModel::CachedImageModel(
    const CachedImageModel& other) = default;

ClipboardHistoryResourceManager::CachedImageModel&
ClipboardHistoryResourceManager::CachedImageModel::operator=(
    const CachedImageModel&) = default;

ClipboardHistoryResourceManager::CachedImageModel::~CachedImageModel() =
    default;

void ClipboardHistoryResourceManager::CacheImageModel(
    const base::UnguessableToken& id,
    ui::ImageModel image_model) {
  auto cached_image_model = base::ConstCastIterator(
      cached_image_models_, FindCachedImageModelForId(id));
  if (cached_image_model == cached_image_models_.end())
    return;

  cached_image_model->image_model = std::move(image_model);

  for (auto& observer : observers_) {
    observer.OnCachedImageModelUpdated(
        cached_image_model->clipboard_history_item_ids);
  }
}

std::vector<ClipboardHistoryResourceManager::CachedImageModel>::const_iterator
ClipboardHistoryResourceManager::FindCachedImageModelForId(
    const base::UnguessableToken& id) const {
  return std::find_if(cached_image_models_.cbegin(),
                      cached_image_models_.cend(),
                      [&](const auto& cached_image_model) {
                        return cached_image_model.id == id;
                      });
}

std::vector<ClipboardHistoryResourceManager::CachedImageModel>::const_iterator
ClipboardHistoryResourceManager::FindCachedImageModelForItem(
    const ClipboardHistoryItem& item) const {
  return std::find_if(
      cached_image_models_.cbegin(), cached_image_models_.cend(),
      [&](const auto& cached_image_model) {
        return base::Contains(cached_image_model.clipboard_history_item_ids,
                              item.id());
      });
}

void ClipboardHistoryResourceManager::CancelUnfinishedRequests() {
  for (const auto& cached_image_model : cached_image_models_) {
    if (cached_image_model.image_model.IsEmpty())
      ClipboardImageModelFactory::Get()->CancelRequest(cached_image_model.id);
  }
}

void ClipboardHistoryResourceManager::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item) {
  // For items that will be represented by their rendered HTML, we need to do
  // some prep work to pre-render and cache an image model.
  if (ClipboardHistoryUtil::CalculateMainFormat(item.data()) !=
      ui::ClipboardInternalFormat::kHtml) {
    return;
  }

  const auto& items = clipboard_history_->GetItems();

  // See if we have an |existing| item that will render the same as |item|.
  auto it = std::find_if(items.begin(), items.end(), [&](const auto& existing) {
    return &existing != &item && existing.data().bitmap().isNull() &&
           existing.data().markup_data() == item.data().markup_data();
  });

  // If we don't have an existing image model in the cache, create one and
  // instruct ClipboardImageModelFactory to render it. Note that the factory may
  // or may not start rendering immediately depending on its activation status.
  if (it == items.end()) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    CachedImageModel cached_image_model;
    cached_image_model.id = id;
    cached_image_model.clipboard_history_item_ids.push_back(item.id());
    cached_image_models_.push_back(std::move(cached_image_model));

    ClipboardImageModelFactory::Get()->Render(
        id, item.data().markup_data(),
        base::BindOnce(&ClipboardHistoryResourceManager::CacheImageModel,
                       weak_factory_.GetWeakPtr(), id));
    return;
  }
  // If we do have an existing model, we need only to update its usages.
  auto cached_image_model = base::ConstCastIterator(
      cached_image_models_, FindCachedImageModelForItem(*it));
  DCHECK(cached_image_model != cached_image_models_.end());
  cached_image_model->clipboard_history_item_ids.push_back(item.id());
}

void ClipboardHistoryResourceManager::OnClipboardHistoryItemRemoved(
    const ClipboardHistoryItem& item) {
  // For items that will not be represented by their rendered HTML, do nothing.
  if (ClipboardHistoryUtil::CalculateMainFormat(item.data()) !=
      ui::ClipboardInternalFormat::kHtml) {
    return;
  }

  // We should have an image model in the cache.
  auto cached_image_model = base::ConstCastIterator(
      cached_image_models_, FindCachedImageModelForItem(item));

  DCHECK(cached_image_model != cached_image_models_.end());

  // Update usages.
  base::Erase(cached_image_model->clipboard_history_item_ids, item.id());
  if (!cached_image_model->clipboard_history_item_ids.empty())
    return;

  // If the ImageModel was never rendered, cancel the request.
  if (cached_image_model->image_model.IsEmpty())
    ClipboardImageModelFactory::Get()->CancelRequest(cached_image_model->id);

  // If the cached image model is no longer in use, it can be erased.
  cached_image_models_.erase(cached_image_model);
}

void ClipboardHistoryResourceManager::OnClipboardHistoryCleared() {
  CancelUnfinishedRequests();
  cached_image_models_ = std::vector<CachedImageModel>();
}

}  // namespace ash
