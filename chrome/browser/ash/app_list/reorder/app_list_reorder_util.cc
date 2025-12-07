// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/reorder/app_list_reorder_util.h"

#include "ash/strings/grit/ash_strings.h"
#include "base/i18n/string_compare.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/app_icon_color_cache/app_icon_color_cache.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {
namespace reorder {
namespace {

std::u16string GetItemName(const std::string& name, bool is_folder) {
  if (is_folder && name.empty())
    return l10n_util::GetStringUTF16(IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER);
  return base::UTF8ToUTF16(name);
}

}  // namespace

constexpr float kOrderResetThreshold = 0.2f;

// ReorderParam ----------------------------------------------------------------

ReorderParam::ReorderParam(const std::string& new_sync_item_id,
                           const syncer::StringOrdinal& new_ordinal)
    : sync_item_id(new_sync_item_id), ordinal(new_ordinal) {}

ReorderParam::ReorderParam(const ReorderParam&) = default;

ReorderParam::~ReorderParam() = default;

// SyncItemWrapper<std::u16string> ---------------------------------------------

template <>
SyncItemWrapper<std::u16string>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item)
    : id(sync_item.item_id),
      item_ordinal(sync_item.item_ordinal),
      is_folder(sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER),
      key_attribute(GetItemName(sync_item.item_name, is_folder)) {}

template <>
SyncItemWrapper<std::u16string>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata)
    : id(metadata.id),
      item_ordinal(metadata.position),
      is_folder(metadata.is_folder),
      key_attribute(GetItemName(metadata.name, is_folder)) {}

// SyncItemWrapper<ash::IconColor> ---------------------------------------------

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item)
    : id(sync_item.item_id),
      item_ordinal(sync_item.item_ordinal),
      is_folder(sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER),
      key_attribute(sync_item.item_color) {}

template <>
SyncItemWrapper<ash::IconColor>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata)
    : id(metadata.id),
      item_ordinal(metadata.position),
      is_folder(metadata.is_folder),
      key_attribute(metadata.icon_color) {}

// EphemeralAwareName ----------------------------------------------------------

EphemeralAwareName::EphemeralAwareName(bool is_ephemeral, std::u16string name)
    : is_ephemeral(is_ephemeral), name(name) {}
EphemeralAwareName::~EphemeralAwareName() = default;

// SyncItemWrapper<EphemeralAwareName> -----------------------------------------

template <>
SyncItemWrapper<EphemeralAwareName>::SyncItemWrapper(
    const AppListSyncableService::SyncItem& sync_item)
    : id(sync_item.item_id),
      item_ordinal(sync_item.item_ordinal),
      is_folder(sync_item.item_type == sync_pb::AppListSpecifics::TYPE_FOLDER),
      key_attribute(sync_item.is_ephemeral,
                    GetItemName(sync_item.item_name, is_folder)) {}

template <>
SyncItemWrapper<EphemeralAwareName>::SyncItemWrapper(
    const ash::AppListItemMetadata& metadata)
    : id(metadata.id),
      item_ordinal(metadata.position),
      is_folder(metadata.is_folder),
      key_attribute(metadata.is_ephemeral,
                    GetItemName(metadata.name, is_folder)) {}

// IconColorWrapperComparator -------------------------------------------------

IconColorWrapperComparator::IconColorWrapperComparator() = default;

bool IconColorWrapperComparator::operator()(
    const reorder::SyncItemWrapper<ash::IconColor>& lhs,
    const reorder::SyncItemWrapper<ash::IconColor>& rhs) const {
  // Folders are placed at the bottom of the app list in color sort.
  if (lhs.is_folder != rhs.is_folder)
    return rhs.is_folder;

  if (lhs.key_attribute != rhs.key_attribute)
    return lhs.key_attribute < rhs.key_attribute;

  const syncer::StringOrdinal& lhs_ordinal = lhs.item_ordinal;
  const syncer::StringOrdinal& rhs_ordinal = rhs.item_ordinal;
  if (lhs_ordinal.IsValid() && rhs_ordinal.IsValid() &&
      !lhs_ordinal.Equals(rhs_ordinal)) {
    lhs.item_ordinal.LessThan(rhs.item_ordinal);
  }

  // Compare ids so that sorting with this comparator is stable.
  return lhs.id < rhs.id;
}

// StringWrapperComparator ----------------------------------------------------

StringWrapperComparator::StringWrapperComparator(bool increasing,
                                                 icu::Collator* collator)
    : increasing_(increasing), collator_(collator) {}

bool StringWrapperComparator::operator()(
    const reorder::SyncItemWrapper<std::u16string>& lhs,
    const reorder::SyncItemWrapper<std::u16string>& rhs) const {
  // If the collator is not created successfully, compare the string values
  // using operators.
  if (!collator_) {
    if (increasing_)
      return lhs.key_attribute < rhs.key_attribute;

    return lhs.key_attribute > rhs.key_attribute;
  }

  UCollationResult result = base::i18n::CompareString16WithCollator(
      *collator_, lhs.key_attribute, rhs.key_attribute);
  if (increasing_)
    return result == UCOL_LESS;

  return result == UCOL_GREATER;
}

// EphemeralStateAndNameComparator ---------------------------------------------

EphemeralStateAndNameComparator::EphemeralStateAndNameComparator(
    icu::Collator* collator)
    : collator_(collator) {}

bool EphemeralStateAndNameComparator::operator()(
    const reorder::SyncItemWrapper<EphemeralAwareName>& lhs,
    const reorder::SyncItemWrapper<EphemeralAwareName>& rhs) const {
  if (lhs.key_attribute.is_ephemeral != rhs.key_attribute.is_ephemeral)
    return lhs.key_attribute.is_ephemeral > rhs.key_attribute.is_ephemeral;

  if (!collator_)
    return lhs.key_attribute.name < rhs.key_attribute.name;

  UCollationResult result = base::i18n::CompareString16WithCollator(
      *collator_, lhs.key_attribute.name, rhs.key_attribute.name);
  return result == UCOL_LESS;
}
}  // namespace reorder
}  // namespace app_list
