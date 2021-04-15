// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extension.h"

#include "base/strings/string_number_conversions.h"

SystemExtension::SystemExtension() = default;

SystemExtension::~SystemExtension() = default;

// static
std::string SystemExtension::IdToString(const SystemExtensionId& id) {
  std::string id_str;
  for (uint8_t i : id) {
    id_str += base::NumberToString(i);
  }
  return id_str;
}
