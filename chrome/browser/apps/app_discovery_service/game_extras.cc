// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_extras.h"

#include <memory>

namespace apps {

GameExtras::GameExtras(
    const absl::optional<std::vector<std::u16string>>& platforms,
    Source source,
    const GURL& icon_url)
    : platforms_(platforms), source_(source), icon_url_(icon_url) {}

GameExtras::GameExtras(const GameExtras&) = default;

GameExtras::~GameExtras() = default;

std::unique_ptr<SourceExtras> GameExtras::Clone() {
  return std::make_unique<GameExtras>(*this);
}

const absl::optional<std::vector<std::u16string>>& GameExtras::GetPlatforms()
    const {
  return platforms_;
}

GameExtras::Source GameExtras::GetSource() const {
  return source_;
}

const GURL& GameExtras::GetIconUrl() const {
  return icon_url_;
}

GameExtras* GameExtras::AsGameExtras() {
  return this;
}

}  // namespace apps
