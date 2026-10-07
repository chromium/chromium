// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPIC_FEEDBACK_CONVERSIONS_H_
#define CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPIC_FEEDBACK_CONVERSIONS_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-forward.h"

namespace context_hub {

// Conversions between the fishfood Topic feedback mojom structs and the
// base::Value form persisted under prefs::kContextHubTopicsFishfoodFeedback.
// Each topic's feedback is stored as one dictionary keyed by its topic id.

// Serializes `feedback` (minus its `id`, which is the enclosing pref key).
base::DictValue TopicFeedbackToDict(
    const browser::context_hub::mojom::TopicFeedback& feedback);

// Deserializes the dictionary stored for `topic_id`. Returns null only if the
// snapshot's required fields (title, time_rated) are missing or malformed.
// Everything else is parsed leniently: absent collections read as empty, and
// malformed or unknown elements (e.g. a defect category added by a newer
// Chrome) are skipped rather than failing the entry.
browser::context_hub::mojom::TopicFeedbackPtr TopicFeedbackFromDict(
    const std::string& topic_id,
    const base::DictValue& dict);

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_TOPICS_TOPIC_FEEDBACK_CONVERSIONS_H_
