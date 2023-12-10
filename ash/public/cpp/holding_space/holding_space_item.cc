// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
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

// HoldingSpaceItem::InProgressCommand -----------------------------------------

HoldingSpaceItem::InProgressCommand::InProgressCommand(
    HoldingSpaceCommandId command_id,
    int label_id,
    const gfx::VectorIcon* icon,
    Handler handler)
    : command_id(command_id),
      label_id(label_id),
      icon(icon),
      handler(std::move(handler)) {
  DCHECK(holding_space_util::IsInProgressCommand(command_id));
}

HoldingSpaceItem::InProgressCommand::InProgressCommand(
    const InProgressCommand& other)
    : command_id(other.command_id),
      label_id(other.label_id),
      icon(other.icon),
      handler(other.handler) {}

HoldingSpaceItem::InProgressCommand::~InProgressCommand() = default;

HoldingSpaceItem::InProgressCommand&
HoldingSpaceItem::InProgressCommand::operator=(const InProgressCommand& other) =
    default;

bool HoldingSpaceItem::InProgressCommand::operator==(
    const InProgressCommand& other) const {
  return command_id == other.command_id && label_id == other.label_id &&
         icon == other.icon && handler == other.handler;
}

// HoldingSpaceItem ------------------------------------------------------------

HoldingSpaceItem::~HoldingSpaceItem() {
  deletion_callback_list_.Notify();
}

bool HoldingSpaceItem::operator==(const HoldingSpaceItem& rhs) const {
  return type_ == rhs.type_ && id_ == rhs.id_ && file_ == rhs.file_ &&
         text_ == rhs.text_ && secondary_text_ == rhs.secondary_text_ &&
         secondary_text_color_id_ == rhs.secondary_text_color_id_ &&
         *image_ == *rhs.image_ && progress_ == rhs.progress_ &&
         in_progress_commands_ == rhs.in_progress_commands_;
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const HoldingSpaceFile& file,
    ImageResolver image_resolver) {
  return CreateFileBackedItem(type, file, HoldingSpaceProgress(),
                              std::move(image_resolver));
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const HoldingSpaceFile& file,
    const HoldingSpaceProgress& progress,
    ImageResolver image_resolver) {
  DCHECK(!file.file_system_url.is_empty());

  // Note: std::make_unique does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, /*id=*/base::UnguessableToken::Create().ToString(), file,
      std::move(image_resolver).Run(type, file.file_path), progress));
}

// static
bool HoldingSpaceItem::IsCameraAppType(HoldingSpaceItem::Type type) {
  switch (type) {
    case Type::kCameraAppPhoto:
    case Type::kCameraAppScanJpg:
    case Type::kCameraAppScanPdf:
    case Type::kCameraAppVideoGif:
    case Type::kCameraAppVideoMp4:
      return true;
    case Type::kArcDownload:
    case Type::kDownload:
    case Type::kDiagnosticsLog:
    case Type::kDriveSuggestion:
    case Type::kLacrosDownload:
    case Type::kLocalSuggestion:
    case Type::kNearbyShare:
    case Type::kPhoneHubCameraRoll:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
    case Type::kScreenRecording:
    case Type::kScreenRecordingGif:
    case Type::kScreenshot:
      return false;
  }
}

// static
bool HoldingSpaceItem::IsDownloadType(HoldingSpaceItem::Type type) {
  switch (type) {
    case Type::kArcDownload:
    case Type::kDownload:
    case Type::kLacrosDownload:
      return true;
    case Type::kCameraAppPhoto:
    case Type::kCameraAppScanJpg:
    case Type::kCameraAppScanPdf:
    case Type::kCameraAppVideoGif:
    case Type::kCameraAppVideoMp4:
    case Type::kDiagnosticsLog:
    case Type::kDriveSuggestion:
    case Type::kLocalSuggestion:
    case Type::kNearbyShare:
    case Type::kPhoneHubCameraRoll:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
    case Type::kScreenRecording:
    case Type::kScreenRecordingGif:
    case Type::kScreenshot:
      return false;
  }
}

// static
bool HoldingSpaceItem::IsScreenCaptureType(HoldingSpaceItem::Type type) {
  switch (type) {
    case Type::kScreenRecording:
    case Type::kScreenRecordingGif:
    case Type::kScreenshot:
      return true;
    case Type::kArcDownload:
    case Type::kCameraAppPhoto:
    case Type::kCameraAppScanJpg:
    case Type::kCameraAppScanPdf:
    case Type::kCameraAppVideoGif:
    case Type::kCameraAppVideoMp4:
    case Type::kDiagnosticsLog:
    case Type::kDownload:
    case Type::kDriveSuggestion:
    case Type::kLacrosDownload:
    case Type::kLocalSuggestion:
    case Type::kNearbyShare:
    case Type::kPhoneHubCameraRoll:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
      return false;
  }
}

// static
bool HoldingSpaceItem::IsSuggestionType(HoldingSpaceItem::Type type) {
  switch (type) {
    case Type::kDriveSuggestion:
    case Type::kLocalSuggestion:
      return true;
    case Type::kArcDownload:
    case Type::kCameraAppPhoto:
    case Type::kCameraAppScanJpg:
    case Type::kCameraAppScanPdf:
    case Type::kCameraAppVideoGif:
    case Type::kCameraAppVideoMp4:
    case Type::kDiagnosticsLog:
    case Type::kDownload:
    case Type::kLacrosDownload:
    case Type::kNearbyShare:
    case Type::kPhoneHubCameraRoll:
    case Type::kPinnedFile:
    case Type::kPrintedPdf:
    case Type::kScan:
    case Type::kScreenRecording:
    case Type::kScreenRecordingGif:
    case Type::kScreenshot:
      return false;
  }
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::Deserialize(
    const base::Value::Dict& dict,
    ImageResolver image_resolver) {
  const std::optional<int> version = dict.FindInt(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const Type type = DeserializeType(dict);
  const base::FilePath file_path = DeserializeFilePath(dict);

  // NOTE: `std::make_unique` does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, DeserializeId(dict),
      HoldingSpaceFile(file_path, HoldingSpaceFile::FileSystemType::kUnknown,
                       /*file_system_url=*/GURL()),
      std::move(image_resolver).Run(type, file_path), HoldingSpaceProgress()));
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
const std::string& HoldingSpaceItem::DeserializeId(
    const base::Value::Dict& dict) {
  const std::optional<int> version = dict.FindInt(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const std::string* id = dict.FindString(kIdPath);
  DCHECK(id);

  return *id;
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
base::FilePath HoldingSpaceItem::DeserializeFilePath(
    const base::Value::Dict& dict) {
  const std::optional<int> version = dict.FindInt(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const std::optional<base::FilePath> file_path =
      base::ValueToFilePath(dict.Find(kFilePathPath));
  DCHECK(file_path.has_value());

  return file_path.value();
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
HoldingSpaceItem::Type HoldingSpaceItem::DeserializeType(
    const base::Value::Dict& dict) {
  const std::optional<int> version = dict.FindInt(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  return static_cast<Type>(dict.FindInt(kTypePath).value());
}

// NOTE: This method must remain in sync with `Deserialize()`. The
// return value will be written to preferences so this implementation must
// maintain backwards compatibility so long as `kVersion` remains unchanged.
base::Value::Dict HoldingSpaceItem::Serialize() const {
  base::Value::Dict dict;
  dict.Set(kVersionPath, kVersion);
  dict.Set(kTypePath, static_cast<int>(type_));
  dict.Set(kIdPath, id_);
  dict.Set(kFilePathPath, base::FilePathToValue(file_.file_path));
  return dict;
}

base::CallbackListSubscription HoldingSpaceItem::AddDeletionCallback(
    base::RepeatingClosureList::CallbackType callback) const {
  return deletion_callback_list_.Add(std::move(callback));
}

bool HoldingSpaceItem::IsInitialized() const {
  return !file_.file_system_url.is_empty();
}

void HoldingSpaceItem::Initialize(const HoldingSpaceFile& file) {
  DCHECK(!IsInitialized());
  DCHECK(!file.file_system_url.is_empty());
  file_ = file;
}

bool HoldingSpaceItem::SetBackingFile(const HoldingSpaceFile& file) {
  if (file_ == file) {
    return false;
  }

  file_ = file;
  image_->UpdateBackingFilePath(file_.file_path);

  return true;
}

std::u16string HoldingSpaceItem::GetText() const {
  return text_.value_or(file_.file_path.BaseName().LossyDisplayName());
}

bool HoldingSpaceItem::SetText(const std::optional<std::u16string>& text) {
  if (text_ == text)
    return false;

  text_ = text;
  return true;
}

bool HoldingSpaceItem::SetSecondaryText(
    const std::optional<std::u16string>& secondary_text) {
  if (secondary_text_ == secondary_text)
    return false;

  secondary_text_ = secondary_text;
  return true;
}

bool HoldingSpaceItem::SetSecondaryTextColorId(
    const std::optional<ui::ColorId>& secondary_text_color_id) {
  if (secondary_text_color_id_ == secondary_text_color_id)
    return false;

  secondary_text_color_id_ = secondary_text_color_id;
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
    const std::optional<std::u16string>& accessible_name) {
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
    in_progress_commands_.clear();

  return true;
}

bool HoldingSpaceItem::SetInProgressCommands(
    std::vector<InProgressCommand> in_progress_commands) {
  DCHECK(base::ranges::all_of(in_progress_commands,
                              [](const InProgressCommand& in_progress_command) {
                                return holding_space_util::IsInProgressCommand(
                                    in_progress_command.command_id);
                              }));

  if (progress_.IsComplete() || in_progress_commands_ == in_progress_commands)
    return false;

  in_progress_commands_ = in_progress_commands;
  return true;
}

void HoldingSpaceItem::InvalidateImage() {
  if (image_)
    image_->Invalidate();
}

HoldingSpaceItem::HoldingSpaceItem(Type type,
                                   const std::string& id,
                                   const HoldingSpaceFile& file,
                                   std::unique_ptr<HoldingSpaceImage> image,
                                   const HoldingSpaceProgress& progress)
    : type_(type),
      id_(id),
      file_(file),
      image_(std::move(image)),
      progress_(progress) {}

}  // namespace ash
