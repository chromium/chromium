// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_query.h"

namespace ash {

// AssistantNullQuery ----------------------------------------------------------

bool AssistantNullQuery::Empty() const {
  return true;
}

// AssistantTextQuery ----------------------------------------------------------

bool AssistantTextQuery::Empty() const {
  return text_.empty();
}

// AssistantVoiceQuery ---------------------------------------------------------

bool AssistantVoiceQuery::Empty() const {
  return high_confidence_speech_.empty() && low_confidence_speech_.empty();
}

}  // namespace ash
