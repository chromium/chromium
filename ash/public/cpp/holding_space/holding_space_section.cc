// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_section.h"

#include <map>

#include "base/containers/contains.h"
#include "base/no_destructor.h"

namespace ash {
namespace {

// Returns whether the specified `sections_by_id` pass validation.
bool IsValid(const std::map<HoldingSpaceSectionId, HoldingSpaceSection>&
                 sections_by_id) {
#if DCHECK_IS_ON()
  // There should be no overlap of `supported_types` between `section`s.
  std::set<HoldingSpaceItem::Type> types;
  for (const auto& [id, section] : sections_by_id) {
    for (const auto& type : section.supported_types) {
      if (!types.insert(type).second)
        return false;
    }
  }
#endif  // DCHECK_IS_ON()
  return true;
}

// Creates the map of all holding space sections to their respective IDs.
std::map<HoldingSpaceSectionId, HoldingSpaceSection> CreateSectionsById() {
  std::map<HoldingSpaceSectionId, HoldingSpaceSection> sections_by_id;

  // Downloads.
  sections_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(HoldingSpaceSectionId::kDownloads),
      std::forward_as_tuple(
          /*id=*/HoldingSpaceSectionId::kDownloads,
          /*supported_types=*/
          std::set<HoldingSpaceItem::Type>({
              HoldingSpaceItem::Type::kArcDownload,
              HoldingSpaceItem::Type::kDiagnosticsLog,
              HoldingSpaceItem::Type::kDownload,
              HoldingSpaceItem::Type::kLacrosDownload,
              HoldingSpaceItem::Type::kNearbyShare,
              HoldingSpaceItem::Type::kPhotoshopWeb,
              HoldingSpaceItem::Type::kPrintedPdf,
              HoldingSpaceItem::Type::kScan,
              HoldingSpaceItem::Type::kPhoneHubCameraRoll,
          }),
          /*max_visible_item_count=*/
          std::make_optional<size_t>(4u)));

  // Pinned files.
  sections_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(HoldingSpaceSectionId::kPinnedFiles),
      std::forward_as_tuple(
          /*id=*/HoldingSpaceSectionId::kPinnedFiles,
          /*supported_types=*/
          std::set<HoldingSpaceItem::Type>({
              HoldingSpaceItem::Type::kPinnedFile,
          }),
          /*max_visible_item_count=*/std::optional<size_t>()));

  // Screen captures.
  sections_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(HoldingSpaceSectionId::kScreenCaptures),
      std::forward_as_tuple(
          /*id=*/HoldingSpaceSectionId::kScreenCaptures,
          /*supported_types=*/
          std::set<HoldingSpaceItem::Type>({
              HoldingSpaceItem::Type::kScreenRecording,
              HoldingSpaceItem::Type::kScreenRecordingGif,
              HoldingSpaceItem::Type::kScreenshot,
          }),
          /*max_visible_item_count=*/
          std::make_optional<size_t>(3u)));

  // Suggestions.
  sections_by_id.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(HoldingSpaceSectionId::kSuggestions),
      std::forward_as_tuple(
          /*id=*/HoldingSpaceSectionId::kSuggestions,
          /*supported_types=*/
          std::set<HoldingSpaceItem::Type>({
              HoldingSpaceItem::Type::kDriveSuggestion,
              HoldingSpaceItem::Type::kLocalSuggestion,
          }),
          /*max_visible_item_count=*/
          std::make_optional<size_t>(4u)));

  DCHECK(IsValid(sections_by_id));
  return sections_by_id;
}

// Returns all holding space sections mapped to their respective IDs.
std::map<HoldingSpaceSectionId, HoldingSpaceSection>& GetSectionsById() {
  static base::NoDestructor<
      std::map<HoldingSpaceSectionId, HoldingSpaceSection>>
      sections_by_id(CreateSectionsById());
  return *sections_by_id;
}

}  // namespace

// HoldingSpaceSection ---------------------------------------------------------

HoldingSpaceSection::HoldingSpaceSection(
    HoldingSpaceSectionId id,
    std::set<HoldingSpaceItem::Type> supported_types,
    std::optional<size_t> max_visible_item_count)
    : id(id),
      supported_types(std::move(supported_types)),
      max_visible_item_count(max_visible_item_count) {}

HoldingSpaceSection::~HoldingSpaceSection() = default;

// Utilities -------------------------------------------------------------------

const HoldingSpaceSection* GetHoldingSpaceSection(HoldingSpaceItem::Type type) {
  for (const auto& [id, section] : GetSectionsById()) {
    if (base::Contains(section.supported_types, type))
      return &section;
  }
  return nullptr;
}

const HoldingSpaceSection* GetHoldingSpaceSection(HoldingSpaceSectionId id) {
  const auto& sections_by_id = GetSectionsById();
  const auto it = sections_by_id.find(id);
  return it != sections_by_id.end() ? &it->second : nullptr;
}

}  // namespace ash
