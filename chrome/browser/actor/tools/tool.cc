// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool.h"

#include "chrome/common/actor/action_result.h"

namespace actor {

mojom::ActionResultPtr Tool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation)
    const {
  // TODO(crbug.com/411462297): This should be made pure-virtual.
  return MakeOkResult();
}

GURL Tool::JournalURL() const {
  return GURL::EmptyGURL();
}

}  // namespace actor
