// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DETACHABLE_BASE_DETACHABLE_BASE_PAIRING_STATUS_H_
#define ASH_DETACHABLE_BASE_DETACHABLE_BASE_PAIRING_STATUS_H_

#include "ash/ash_export.h"

namespace ash {

// Enum describing current detachable base device pairing status.
enum class ASH_EXPORT DetachableBasePairingStatus {
  // A detachable base is not currently paired.
  kNone,

  // A detachable base is paired, and successfully authenticated.
  kAuthenticated,

  // A detachable base is paired, but it was not successfully authenticated.
  kNotAuthenticated,

  // A detachable base is paired, but it was not verified to be a valid base -
  // e.g. unlike bases in kNotAuthenticated state, the paired device might not
  // support authentication at all.
  kInvalidDevice,
};

}  // namespace ash

#endif  // ASH_DETACHABLE_BASE_DETACHABLE_BASE_PAIRING_STATUS_H_
