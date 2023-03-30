// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"

#include "base/uuid.h"
namespace base {

std::string GenerateGUID() {
  return GenerateUuid();
}

bool IsValidGUID(StringPiece input) {
  return IsValidUuid(input);
}

bool IsValidGUIDOutputString(StringPiece input) {
  return IsValidUuidOutputString(input);
}

}  // namespace base
