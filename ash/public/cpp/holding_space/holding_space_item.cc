// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Used to indicate which version of serialization is being used. When
// intentionally breaking backwards compatibility, increment this value and
// perform any necessary conversions in `Deserialize()`.
constexpr int kVersion = 1;

// Preference paths.
// NOTE: As these paths are written to preferences, changes must ensure
// backwards compatibility. When intentionally breaking backwards compatibility,
// increment `kVersion` and perform any needed conversions in `Deserialize()`.
constexpr char kFilePathPath[] = "filePath";
constexpr char kIdPath[] = "id";
constexpr char kTypePath[] = "type";
constexpr char kVersionPath[] = "version";

}  // namespace

HoldingSpaceItem::~HoldingSpaceItem() {
  deletion_callback_list_.Notify();
}

bool HoldingSpaceItem::operator==(const HoldingSpaceItem& rhs) const {
  return type_ == rhs.type_ && id_ == rhs.id_ && file_path_ == rhs.file_path_ &&
         file_system_url_ == rhs.file_system_url_ && text_ == rhs.text_ &&
         secondary_text_ == rhs.secondary_text_ &&
         secondary_text_color_ == rhs.secondary_text_color_ &&
         *image_ == *rhs.image_ && progress_ == rhs.progress_ &&
         paused_ == rhs.paused_;
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const base::FilePath& file_path,
    const GURL& file_system_url,
    ImageResolver image_resolver) {
  return CreateFileBackedItem(type, file_path, file_system_url,
                              HoldingSpaceProgress(),
                              std::move(image_resolver));
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const base::FilePath& file_path,
    const GURL& file_system_url,
    const HoldingSpaceProgress& progress,
    ImageResolver image_resolver) {
  DCHECK(!file_system_url.is_empty());

  // Note: std::make_unique does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, /*id=*/base::UnguessableToken::Create().ToString(), file_path,
      file_system_url, std::move(image_resolver).Run(type, file_path),
      progress));
}

// static
bool HoldingSpaceItem::IsDownload(HoldingSpaceItem::Type type) {
  switch (type) {
    case Type::kArcDownload:
    case Type::kDownload:
    case Type::kLacrosDownload:
      return true;
    case Type::kDiagnosticsLog:
    case Type::kNearbyShare:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
    case Type::kScreenRecording:
    case Type::kScreenshot:
    case Type::kPhoneHubCameraRoll:
      return false;
  }
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::Deserialize(
    const base::DictionaryValue& dict,
    ImageResolver image_resolver) {
  const absl::optional<int> version = dict.FindIntPath(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const Type type = static_cast<Type>(dict.FindIntPath(kTypePath).value());
  const base::FilePath file_path = DeserializeFilePath(dict);

  // NOTE: `std::make_unique` does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, DeserializeId(dict), file_path,
      /*file_system_url=*/GURL(),
      std::move(image_resolver).Run(type, file_path), HoldingSpaceProgress()));
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
const std::string& HoldingSpaceItem::DeserializeId(
    const base::DictionaryValue& dict) {
  const absl::optional<int> version = dict.FindIntPath(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const std::string* id = dict.FindStringPath(kIdPath);
  DCHECK(id);

  return *id;
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
base::FilePath HoldingSpaceItem::DeserializeFilePath(
    const base::DictionaryValue& dict) {
  const absl::optional<int> version = dict.FindIntPath(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const absl::optional<base::FilePath> file_path =
      base::ValueToFilePath(dict.FindPath(kFilePathPath));
  DCHECK(file_path.has_value());

  return file_path.value();
}

// NOTE: This method must remain in sync with `Deserialize()`. The
// return value will be written to preferences so this implementation must
// maintain backwards compatibility so long as `kVersion` remains unchanged.
base::DictionaryValue HoldingSpaceItem::Serialize() const {
  base::DictionaryValue dict;
  dict.SetIntPath(kVersionPath, kVersion);
  dict.SetIntPath(kTypePath, static_cast<int>(type_));
  dict.SetStringPath(kIdPath, id_);
  dict.SetPath(kFilePathPath, base::FilePathToValue(file_path_));
  return dict;
}

base::CallbackListSubscription HoldingSpaceItem::AddDeletionCallback(
    base::RepeatingClosureList::CallbackType callback) const {
  return deletion_callback_list_.Add(std::move(callback));
}

bool HoldingSpaceItem::IsInitialized() const {
  return !file_system_url_.is_empty();
}

void HoldingSpaceItem::Initialize(const GURL& file_system_url) {
  DCHECK(!IsInitialized());
  DCHECK(!file_system_url.is_empty());
  file_system_url_ = file_system_url;
}

bool HoldingSpaceItem::SetBackingFile(const base::FilePath& file_path,
                                      const GURL& file_system_url) {
  if (file_path_ == file_path && file_system_url_ == file_system_url)
    return false;

  file_path_ = file_path;
  file_system_url_ = file_system_url;
  image_->UpdateBackingFilePath(file_path);

  return true;
}

std::u16string HoldingSpaceItem::GetText() const {
  return text_.value_or(file_path_.BaseName().LossyDisplayName());
}

bool HoldingSpaceItem::SetText(const absl::optional<std::u16string>& text) {
  if (text_ == text)
    return false;

  text_ = text;
  return true;
}

bool HoldingSpaceItem::SetSecondaryText(
    const absl::optional<std::u16string>& secondary_text) {
  if (secondary_text_ == secondary_text)
    return false;

  secondary_text_ = secondary_text;
  return true;
}

bool HoldingSpaceItem::SetSecondaryTextColor(
    const absl::optional<cros_styles::ColorName>& secondary_text_color) {
  if (secondary_text_color_ == secondary_text_color)
    return false;

  secondary_text_color_ = secondary_text_color;
  return true;
}

std::u16string HoldingSpaceItem::GetAccessibleName() const {
  if (accessible_name_)
    return accessible_name_.value();

  const std::u16string text = GetText();

  if (!secondary_text_)
    return text;

  return l10n_util::GetStringFUTF16(
      IDS_ASH_HOLDING_SPACE_ITEM_A11Y_NAME_AND_TOOLTIP, text,
      secondary_text_.value());
}

bool HoldingSpaceItem::SetAccessibleName(
    const absl::optional<std::u16string>& accessible_name) {
  if (accessible_name_ == accessible_name)
    return false;

  accessible_name_ = accessible_name;
  return true;
}

bool HoldingSpaceItem::SetProgress(const HoldingSpaceProgress& progress) {
  // NOTE: Progress can only be updated for in progress items.
  if (progress_ == progress || progress_.IsComplete())
    return false;

  progress_ = progress;

  if (progress_.IsComplete())
    paused_ = false;

  return true;
}

void HoldingSpaceItem::InvalidateImage() {
  if (image_)
    image_->Invalidate();
}

bool HoldingSpaceItem::IsScreenCapture() const {
  switch (type_) {
    case Type::kScreenRecording:
    case Type::kScreenshot:
      return true;
    case Type::kArcDownload:
    case Type::kDiagnosticsLog:
    case Type::kDownload:
    case Type::kLacrosDownload:
    case Type::kNearbyShare:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
    case Type::kPhoneHubCameraRoll:
      return false;
  }
}

bool HoldingSpaceItem::IsPaused() const {
  return paused_;
}

bool HoldingSpaceItem::SetPaused(bool paused) {
  if (paused_ == paused || progress_.IsComplete())
    return false;

  paused_ = paused;
  return true;
}

HoldingSpaceItem::HoldingSpaceItem(Type type,
                                   const std::string& id,
                                   const base::FilePath& file_path,
                                   const GURL& file_system_url,
                                   std::unique_ptr<HoldingSpaceImage> image,
                                   const HoldingSpaceProgress& progress)
    : type_(type),
      id_(id),
      file_path_(file_path),
      file_system_url_(file_system_url),
      image_(std::move(image)),
      progress_(progress) {}

}  // namespace ash
