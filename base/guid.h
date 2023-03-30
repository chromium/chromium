// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GUID_H_
#define BASE_GUID_H_

#include "base/uuid.h"

// DEPRECATED(crbug.com/1428566): Please use `base/uuid.h`. This file will be
// removed as soon as all places referring to `base::GUID` are replaced with
// `base::Uuid`.

namespace base {

// An alias to allow a gradual transition from `GUID` to `Uuid`
using GUID = Uuid;

// An alias to allow a gradual transition from `GUID` to `UuidHash`.
using GUIDHash = UuidHash;

// DEPREACATED. Aliases for functions in `base/uuid.h`.
BASE_EXPORT std::string GenerateGUID();
BASE_EXPORT bool IsValidGUID(StringPiece input);
BASE_EXPORT bool IsValidGUIDOutputString(StringPiece input);

}  // namespace base

#endif  // BASE_GUID_H_
