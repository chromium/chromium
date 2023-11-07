// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSION_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSION_H_

#include <array>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

// TODO(ortuno): This should be longer.
using SystemExtensionId = std::array<uint8_t, 4>;

enum class SystemExtensionType {
  kWindowManagement,
  kPeripheralPrototype,
  kManagedDeviceHealthServices,
};

struct SystemExtension {
  static bool IsSystemExtensionOrigin(const url::Origin& origin);

  SystemExtension();
  ~SystemExtension();
  SystemExtension(const SystemExtension&) = delete;
  SystemExtension(SystemExtension&&);
  SystemExtension& operator=(const SystemExtension&) = delete;
  SystemExtension& operator=(SystemExtension&&) = default;

  static std::string IdToString(const SystemExtensionId& system_extension_id);
  static absl::optional<SystemExtensionId> StringToId(base::StringPiece id_str);

  // The following fields are specified by the System Extension itself.

  // Currently only kEcho is allowed.
  SystemExtensionType type;
  // Unique identifier for the System Extension.
  SystemExtensionId id;
  // Display name of the System Extension.
  std::string name;
  // Display name of the System Extension to be used where
  // the number of characters is limited.
  absl::optional<std::string> short_name;
  // Entry point to the System Extension.
  GURL service_worker_url;

  // The following fields are constructed from the System Extension's manifest.

  // The System Extension's base URL derived from the type and the id e.g.
  // `chrome-untrusted://system-extension-echo-1234/`
  GURL base_url;

  // Parsed JSON that was used to installed the System Extension.
  base::Value::Dict manifest;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSION_H_
