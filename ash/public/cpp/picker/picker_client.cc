// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/picker/picker_client.h"

namespace ash {

std::optional<ValidGifUrl> ValidGifUrl::Create(const GURL& url) {
  // For now, only allow gifs from tenor.
  // TODO: b/323784358 - Once we know what gifs the picker might show, consider
  // making the method parameters more specific to allowed gif sources.
  if (url.DomainIs("media.tenor.com") && url.SchemeIs(url::kHttpsScheme)) {
    return ValidGifUrl(url);
  }
  return std::nullopt;
}

ValidGifUrl::~ValidGifUrl() = default;

GURL ValidGifUrl::ToGURL() const {
  return url_;
}

ValidGifUrl::ValidGifUrl(GURL url) : url_(url) {}

PickerClient::PickerClient() = default;

PickerClient::~PickerClient() = default;

}  // namespace ash
