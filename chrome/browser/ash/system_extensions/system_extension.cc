// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extension.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"

SystemExtension::SystemExtension() = default;

SystemExtension::~SystemExtension() = default;

// static
std::string SystemExtension::IdToString(const SystemExtensionId& id) {
  return base::HexEncode(id);
}

absl::optional<SystemExtensionId> SystemExtension::StringToId(
    base::StringPiece id_str) {
  SystemExtensionId id;
  if (base::HexStringToSpan(id_str, id))
    return id;
  return absl::nullopt;
}
