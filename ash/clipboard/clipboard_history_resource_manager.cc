// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_resource_manager.h"

#include <string>

#include "ash/clipboard/clipboard_history_util.h"
#include "ash/display/display_util.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

constexpr int kPlaceholderImageWidth = 234;
constexpr int kPlaceholderImageHeight = 74;
constexpr int kPlaceholderImageOutlineCornerRadius = 8;
constexpr int kPlaceholderImageSVGSize = 32;

// Used in histograms, each value corresponds with an underlying placeholder
// string displayed by a ClipboardHistoryTextItemView. Do not reorder entries,
// if you must add to it, add at the end.
enum class ClipboardHistoryPlaceholderStringType {
  kBitmap = 0,
  kHtml = 1,
  kRtf = 2,
  kWebSmartPaste = 3,
  kMaxValue = 3,
};

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
        /*rect=*/{kPlaceholderImageWidth, kPlaceholderImageHeight},
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
std::u16string GetLocalizedString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
      resource_id);
}

// Returns label to display for the file system data contained within |data|.
std::u16string GetLabelForFileSystemData(const ui::ClipboardData& data) {
  // This code should not be reached if `data` doesn't contain file system data.
  std::u16string sources;
  std::vector<base::StringPiece16> source_list;
  clipboard_history_util::GetSplitFileSystemData(data, &source_list, &sources);
  if (sources.empty()) {
    NOTREACHED();
    return std::u16string();
  }

  // Strip path information, so all that's left are file names.
  for (auto it = source_list.begin(); it != source_list.end(); ++it)
    *it = it->substr(it->find_last_of(u"/") + 1);

  // Join file names, unescaping encoded character sequences for display. This
  // ensures that "My%20File.txt" will display as "My File.txt".
  return base::UTF8ToUTF16(base::UnescapeURLComponent(
      base::UTF16ToUTF8(base::JoinString(source_list, u", ")),
      base::UnescapeRule::SPACES));
}

void RecordPlaceholderString(ClipboardHistoryPlaceholderStringType type) {
  base::UmaHistogramEnumeration(
      "Ash.ClipboardHistory.ContextMenu.ShowPlaceholderString", type);
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

std::u16string ClipboardHistoryResourceManager::GetLabel(
    const ClipboardHistoryItem& item) const {
  const ui::ClipboardData& data = item.data();
  switch (clipboard_history_util::CalculateMainFormat(data).value()) {
    case ui::ClipboardInternalFormat::kPng:
      RecordPlaceholderString(ClipboardHistoryPlaceholderStringType::kBitmap);
      return GetLocalizedString(IDS_CLIPBOARD_MENU_IMAGE);
    case ui::ClipboardInternalFormat::kText:
      return base::UTF8ToUTF16(data.text());
    case ui::ClipboardInternalFormat::kHtml:
      // Show plain-text if it exists, otherwise show the placeholder.
      if (!data.text().empty())
        return base::UTF8ToUTF16(data.text());
      RecordPlaceholderString(ClipboardHistoryPlaceholderStringType::kHtml);
      return GetLocalizedString(IDS_CLIPBOARD_MENU_HTML);
    case ui::ClipboardInternalFormat::kSvg:
      return base::UTF8ToUTF16(data.svg_data());
    case ui::ClipboardInternalFormat::kRtf:
      RecordPlaceholderString(ClipboardHistoryPlaceholderStringType::kRtf);
      return GetLocalizedString(IDS_CLIPBOARD_MENU_RTF_CONTENT);
    case ui::ClipboardInternalFormat::kBookmark:
      return base::UTF8ToUTF16(data.bookmark_title());
    case ui::ClipboardInternalFormat::kWeb:
      RecordPlaceholderString(
          ClipboardHistoryPlaceholderStringType::kWebSmartPaste);
      return GetLocalizedString(IDS_CLIPBOARD_MENU_WEB_SMART_PASTE);
    case ui::ClipboardInternalFormat::kFilenames:
    case ui::ClipboardInternalFormat::kCustom:
      // Currently the only supported type of custom data is file system data.
      return GetLabelForFileSystemData(data);
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
  return base::ranges::find(
      cached_image_models_, id,
      &ClipboardHistoryResourceManager::CachedImageModel::id);
}

std::vector<ClipboardHistoryResourceManager::CachedImageModel>::const_iterator
ClipboardHistoryResourceManager::FindCachedImageModelForItem(
    const ClipboardHistoryItem& item) const {
  return base::ranges::find_if(
      cached_image_models_, [&](const auto& cached_image_model) {
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
    const ClipboardHistoryItem& item,
    bool is_duplicate) {
  // If this item is a duplicate then there is no new item to render.
  if (is_duplicate)
    return;

  // For items that will be represented by their rendered HTML, we need to do
  // some prep work to pre-render and cache an image model.
  if (clipboard_history_util::CalculateDisplayFormat(item.data()) !=
      clipboard_history_util::DisplayFormat::kHtml) {
    return;
  }

  const auto& items = clipboard_history_->GetItems();

  // See if we have an |existing| item that will render the same as |item|.
  auto it = base::ranges::find_if(items, [&](const auto& existing) {
    return &existing != &item &&
           !(existing.data().format() &
             static_cast<int>(ui::ClipboardInternalFormat::kPng)) &&
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

    // `text_input_client` can be nullptr in tests.
    const auto* text_input_client =
        ash::GetWindowTreeHostForDisplay(
            display::Screen::GetScreen()->GetPrimaryDisplay().id())
            ->GetInputMethod()
            ->GetTextInputClient();

    const gfx::Rect bounding_box =
        text_input_client ? text_input_client->GetSelectionBoundingBox()
                          : gfx::Rect();
    ClipboardImageModelFactory::Get()->Render(
        id, item.data().markup_data(),
        IsRectContainedByAnyDisplay(bounding_box) ? bounding_box.size()
                                                  : gfx::Size(),
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
  if (clipboard_history_util::CalculateDisplayFormat(item.data()) !=
      clipboard_history_util::DisplayFormat::kHtml) {
    return;
  }

  // We should have an image model in the cache.
  auto cached_image_model = base::ConstCastIterator(
      cached_image_models_, FindCachedImageModelForItem(item));

  DCHECK(cached_image_model != cached_image_models_.end());
  if (cached_image_model == cached_image_models_.end())
    return;

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
