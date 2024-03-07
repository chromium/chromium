// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"

#include <string>

#include "url/gurl.h"

namespace mahi {
ActionType MatchButtonTypeToActionType(const ButtonType button_type) {
  switch (button_type) {
    case ButtonType::kSummary:
      return ActionType::kSummary;
    case ButtonType::kOutline:
      return ActionType::kOutline;
    case ButtonType::kSettings:
      return ActionType::kSettings;
    case ButtonType::kQA:
      return ActionType::kQA;
  }
}

WebContentState::WebContentState(const WebContentState& state) = default;

WebContentState::WebContentState(const GURL& url, const std::u16string& title)
    : url(url), title(title) {}

WebContentState::~WebContentState() = default;

}  // namespace mahi
