// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/notifier_metadata.h"

namespace ash {

NotifierMetadata::NotifierMetadata() = default;
NotifierMetadata::NotifierMetadata(const NotifierMetadata& other) = default;
NotifierMetadata::NotifierMetadata(NotifierMetadata&& other) = default;

NotifierMetadata::NotifierMetadata(
    const message_center::NotifierId& notifier_id,
    const std::u16string& name,
    bool enabled,
    bool enforced,
    const gfx::ImageSkia& icon)
    : notifier_id(notifier_id),
      name(name),
      enabled(enabled),
      enforced(enforced),
      icon(icon) {}

NotifierMetadata::~NotifierMetadata() = default;

NotifierMetadata& NotifierMetadata::operator=(const NotifierMetadata& other) =
    default;
NotifierMetadata& NotifierMetadata::operator=(NotifierMetadata&& other) =
    default;

}  // namespace ash
