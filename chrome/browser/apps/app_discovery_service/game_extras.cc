// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/game_extras.h"

namespace apps {
namespace {

using Source = GameExtras::Source;

}  // namespace

GameExtras::GameExtras(
    const std::string& id,
    const std::u16string& title,
    Source source,
    const absl::optional<std::vector<std::u16string>>& platforms,
    const GURL& icon_url)
    : id_(id),
      title_(title),
      source_(source),
      platforms_(platforms),
      icon_url_(icon_url) {}

GameExtras::~GameExtras() = default;

const std::string& GameExtras::GetId() const {
  return id_;
}

const std::u16string& GameExtras::GetTitle() const {
  return title_;
}

Source GameExtras::GetSource() const {
  return source_;
}

const absl::optional<std::vector<std::u16string>>& GameExtras::GetPlatforms()
    const {
  return platforms_;
}

const GURL& GameExtras::GetIconUrl() const {
  return icon_url_;
}

GameExtras* GameExtras::AsGameExtras() {
  return this;
}

}  // namespace apps
