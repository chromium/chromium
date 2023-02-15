// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

using extensions::DictionaryBuilder;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;

namespace ash {

TestKioskExtensionBuilder::TestKioskExtensionBuilder(
    extensions::Manifest::Type type,
    const std::string& extension_id)
    : type_(type), extension_id_(extension_id) {}

TestKioskExtensionBuilder::~TestKioskExtensionBuilder() = default;

void TestKioskExtensionBuilder::AddSecondaryExtension(const std::string& id) {
  secondary_extensions_.emplace_back(id, absl::nullopt);
}

void TestKioskExtensionBuilder::AddSecondaryExtensionWithEnabledOnLaunch(
    const std::string& id,
    bool enabled_on_launch) {
  secondary_extensions_.emplace_back(id,
                                     absl::optional<bool>(enabled_on_launch));
}

scoped_refptr<const extensions::Extension> TestKioskExtensionBuilder::Build()
    const {
  DictionaryBuilder manifest_builder;
  manifest_builder.Set("name", "Test kiosk app")
      .Set("version", version_)
      .Set("manifest_version", 2);

  base::Value background = base::Value(
      DictionaryBuilder()
          .Set("scripts",
               base::Value(ListBuilder().Append("background.js").Build()))
          .Build());

  switch (type_) {
    case extensions::Manifest::TYPE_PLATFORM_APP:
      manifest_builder.Set(
          "app",
          DictionaryBuilder().Set("background", std::move(background)).Build());
      break;
    case extensions::Manifest::TYPE_EXTENSION:
      manifest_builder.Set("background", std::move(background));
      break;
    default:
      LOG(ERROR) << "Unsupported extension type";
      return nullptr;
  }

  if (kiosk_enabled_) {
    manifest_builder.Set("kiosk_enabled", kiosk_enabled_);
  }

  manifest_builder.Set("offline_enabled", offline_enabled_);

  if (!secondary_extensions_.empty()) {
    ListBuilder secondary_extension_list_builder;
    for (const auto& secondary_extension : secondary_extensions_) {
      DictionaryBuilder secondary_extension_builder;
      secondary_extension_builder.Set("id", secondary_extension.id);
      if (secondary_extension.enabled_on_launch.has_value()) {
        secondary_extension_builder.Set(
            "enabled_on_launch", secondary_extension.enabled_on_launch.value());
      }
      secondary_extension_list_builder.Append(
          secondary_extension_builder.Build());
    }
    manifest_builder.Set("kiosk_secondary_apps",
                         secondary_extension_list_builder.Build());
  }

  return ExtensionBuilder()
      .SetManifest(manifest_builder.Build())
      .SetID(extension_id_)
      .Build();
}

}  // namespace ash
