// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SKILLS_SKILLS_GLIC_MOJOM_UTIL_H_
#define CHROME_BROWSER_SKILLS_SKILLS_GLIC_MOJOM_UTIL_H_

#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/skills/public/skill.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

glic::mojom::SkillSource SyncPbToGlicMojomSkillSource(
    sync_pb::SkillSource source);

sync_pb::SkillSource GlicMojomToSyncPbSkillSource(
    glic::mojom::SkillSource source);

glic::mojom::SkillPreviewPtr SkillToGlicMojomSkillPreview(
    const skills::Skill* skill);

}  // namespace skills

#endif  // CHROME_BROWSER_SKILLS_SKILLS_GLIC_MOJOM_UTIL_H_
