// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_item.h"

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "base/util/values/values_util.h"

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

HoldingSpaceItem::~HoldingSpaceItem() = default;

bool HoldingSpaceItem::operator==(const HoldingSpaceItem& rhs) const {
  return type_ == rhs.type_ && id_ == rhs.id_ && file_path_ == rhs.file_path_ &&
         file_system_url_ == rhs.file_system_url_ && text_ == rhs.text_ &&
         *image_ == *rhs.image_;
}

// static
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::CreateFileBackedItem(
    Type type,
    const base::FilePath& file_path,
    const GURL& file_system_url,
    ImageResolver image_resolver) {
  DCHECK(!file_system_url.is_empty());

  // Note: std::make_unique does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, /*id=*/base::UnguessableToken::Create().ToString(), file_path,
      file_system_url, file_path.BaseName().LossyDisplayName(),
      std::move(image_resolver).Run(type, file_path)));
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
std::unique_ptr<HoldingSpaceItem> HoldingSpaceItem::Deserialize(
    const base::DictionaryValue& dict,
    ImageResolver image_resolver) {
  const base::Optional<int> version = dict.FindIntPath(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const Type type = static_cast<Type>(dict.FindIntPath(kTypePath).value());
  const base::FilePath file_path = DeserializeFilePath(dict);

  // NOTE: `std::make_unique` does not work with private constructors.
  return base::WrapUnique(new HoldingSpaceItem(
      type, DeserializeId(dict), file_path,
      /*file_system_url=*/GURL(), file_path.BaseName().LossyDisplayName(),
      std::move(image_resolver).Run(type, file_path)));
}

// static
// NOTE: This method must remain in sync with `Serialize()`. If multiple
// serialization versions are supported, care must be taken to handle each.
const std::string& HoldingSpaceItem::DeserializeId(
    const base::DictionaryValue& dict) {
  const base::Optional<int> version = dict.FindIntPath(kVersionPath);
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
  const base::Optional<int> version = dict.FindIntPath(kVersionPath);
  DCHECK(version.has_value() && version.value() == kVersion);

  const base::Optional<base::FilePath> file_path =
      util::ValueToFilePath(dict.FindPath(kFilePathPath));
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
  dict.SetPath(kFilePathPath, util::FilePathToValue(file_path_));
  return dict;
}

bool HoldingSpaceItem::IsFinalized() const {
  return !file_system_url_.is_empty();
}

void HoldingSpaceItem::Finalize(const GURL& file_system_url) {
  DCHECK(!IsFinalized());
  DCHECK(!file_system_url.is_empty());
  file_system_url_ = file_system_url;
}

void HoldingSpaceItem::UpdateBackingFile(const base::FilePath& file_path,
                                         const GURL& file_system_url) {
  file_path_ = file_path;
  file_system_url_ = file_system_url;
  text_ = file_path.BaseName().LossyDisplayName();
  image_->UpdateBackingFilePath(file_path);
}

void HoldingSpaceItem::InvalidateImage() {
  if (image_)
    image_->Invalidate();
}

HoldingSpaceItem::HoldingSpaceItem(Type type,
                                   const std::string& id,
                                   const base::FilePath& file_path,
                                   const GURL& file_system_url,
                                   const base::string16& text,
                                   std::unique_ptr<HoldingSpaceImage> image)
    : type_(type),
      id_(id),
      file_path_(file_path),
      file_system_url_(file_system_url),
      text_(text),
      image_(std::move(image)) {}

}  // namespace ash
