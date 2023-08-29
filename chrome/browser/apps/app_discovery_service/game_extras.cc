// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_extras.h"

#include <memory>

#include "base/files/file_path.h"

namespace apps {

GameExtras::GameExtras(const std::u16string& source,
                       const base::FilePath& relative_icon_path,
                       const bool is_icon_masking_allowed,
                       const GURL& deeplink_url)
    : source_(source),
      relative_icon_path_(relative_icon_path),
      is_icon_masking_allowed_(is_icon_masking_allowed),
      deeplink_url_(deeplink_url) {}

GameExtras::GameExtras(const GameExtras&) = default;

GameExtras::~GameExtras() = default;

std::unique_ptr<SourceExtras> GameExtras::Clone() {
  return std::make_unique<GameExtras>(*this);
}

GameExtras* GameExtras::AsGameExtras() {
  return this;
}

const std::u16string& GameExtras::GetSource() const {
  return source_;
}

const base::FilePath& GameExtras::GetRelativeIconPath() const {
  return relative_icon_path_;
}

bool GameExtras::GetIsIconMaskingAllowed() const {
  return is_icon_masking_allowed_;
}

const GURL& GameExtras::GetDeeplinkUrl() const {
  return deeplink_url_;
}

}  // namespace apps
