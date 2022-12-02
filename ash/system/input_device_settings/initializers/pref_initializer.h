// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_PREF_INITIALIZER_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_PREF_INITIALIZER_H_

#include "base/strings/string_piece.h"
#include "components/prefs/pref_service.h"

namespace ash {

class PrefInitializer {
 public:
  virtual void Initialize(PrefService* prefs,
                          const base::StringPiece& device_key) = 0;

 protected:
  virtual ~PrefInitializer() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_INITIALIZERS_PREF_INITIALIZER_H_
