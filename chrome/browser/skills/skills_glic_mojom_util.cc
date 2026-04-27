// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_glic_mojom_util.h"

#include <optional>

#include "base/check.h"
#include "base/notreached.h"
#include "components/skills/public/skill.h"
#include "url/gurl.h"

namespace skills {

glic::mojom::SkillSource SyncPbToGlicMojomSkillSource(
    sync_pb::SkillSource source) {
  switch (source) {
    case sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN:
      return glic::mojom::SkillSource::kUnknown;
    case sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY:
      return glic::mojom::SkillSource::kFirstParty;
    case sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED:
      return glic::mojom::SkillSource::kUserCreated;
    case sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY:
      return glic::mojom::SkillSource::kDerivedFromFirstParty;
  }
  NOTREACHED();
}

sync_pb::SkillSource GlicMojomToSyncPbSkillSource(
    glic::mojom::SkillSource source) {
  switch (source) {
    case glic::mojom::SkillSource::kUnknown:
      return sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;
    case glic::mojom::SkillSource::kFirstParty:
      return sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
    case glic::mojom::SkillSource::kUserCreated:
      return sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
    case glic::mojom::SkillSource::kDerivedFromFirstParty:
      return sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  }
  NOTREACHED();
}

glic::mojom::SkillPreviewPtr SkillToGlicMojomSkillPreview(
    const skills::Skill* skill) {
  CHECK(skill);
  std::optional<GURL> image_url;
  if (!skill->image_url.is_empty()) {
    image_url = skill->image_url;
  }
  std::optional<std::string> curated_by;
  if (!skill->curated_by.empty()) {
    curated_by = skill->curated_by;
  }
  return glic::mojom::SkillPreview::New(
      skill->id, skill->name, skill->icon,
      SyncPbToGlicMojomSkillSource(skill->source), skill->description,
      curated_by, image_url);
}

}  // namespace skills
