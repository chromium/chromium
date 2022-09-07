// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_NOTIFIER_METADATA_H_
#define ASH_PUBLIC_CPP_NOTIFIER_METADATA_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

// Ash-specific information about a Message Center notifier.
struct ASH_PUBLIC_EXPORT NotifierMetadata {
  NotifierMetadata();
  NotifierMetadata(const NotifierMetadata& other);
  NotifierMetadata(NotifierMetadata&& other);
  NotifierMetadata(const message_center::NotifierId& notifier_id,
                   const std::u16string& name,
                   bool enabled,
                   bool enforced,
                   const gfx::ImageSkia& icon);

  ~NotifierMetadata();

  NotifierMetadata& operator=(const NotifierMetadata& other);
  NotifierMetadata& operator=(NotifierMetadata&& other);

  // The notifier (e.g. an extension).
  message_center::NotifierId notifier_id;

  // The user-visible name of the notifier (e.g. an extension's name).
  std::u16string name;

  // True if notifications from the notifier are presently enabled.
  bool enabled = false;

  // True if the setting is enforced by administrator and the user can't change.
  bool enforced = false;

  // An icon displayed next to the name.
  gfx::ImageSkia icon;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_NOTIFIER_METADATA_H_
