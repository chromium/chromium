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
#include "test_kiosk_extension_builder.h"

using extensions::ExtensionBuilder;

namespace ash {

TestKioskExtensionBuilder::TestKioskExtensionBuilder(
    extensions::Manifest::Type type,
    const std::string& extension_id)
    : type_(type), extension_id_(extension_id) {}

TestKioskExtensionBuilder::~TestKioskExtensionBuilder() = default;
TestKioskExtensionBuilder::TestKioskExtensionBuilder(
    TestKioskExtensionBuilder&&) = default;

TestKioskExtensionBuilder& TestKioskExtensionBuilder::AddSecondaryExtension(
    const std::string& id) {
  secondary_extensions_.emplace_back(id, std::nullopt);
  return *this;
}

TestKioskExtensionBuilder&
TestKioskExtensionBuilder::AddSecondaryExtensionWithEnabledOnLaunch(
    const std::string& id,
    bool enabled_on_launch) {
  secondary_extensions_.emplace_back(id,
                                     std::optional<bool>(enabled_on_launch));
  return *this;
}

scoped_refptr<const extensions::Extension> TestKioskExtensionBuilder::Build()
    const {
  auto manifest_builder = base::Value::Dict()
                              .Set("name", "Test kiosk app")
                              .Set("version", version_)
                              .Set("manifest_version", 2);

  base::Value background = base::Value(base::Value::Dict().Set(
      "scripts", base::Value(base::Value::List().Append("background.js"))));

  switch (type_) {
    case extensions::Manifest::TYPE_PLATFORM_APP:
      manifest_builder.Set(
          "app", base::Value::Dict().Set("background", std::move(background)));
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
    base::Value::List secondary_extension_list_builder;
    for (const auto& secondary_extension : secondary_extensions_) {
      auto secondary_extension_builder =
          base::Value::Dict().Set("id", secondary_extension.id);
      if (secondary_extension.enabled_on_launch.has_value()) {
        secondary_extension_builder.Set(
            "enabled_on_launch", secondary_extension.enabled_on_launch.value());
      }
      secondary_extension_list_builder.Append(
          std::move(secondary_extension_builder));
    }
    manifest_builder.Set("kiosk_secondary_apps",
                         std::move(secondary_extension_list_builder));
  }

  return ExtensionBuilder()
      .SetManifest(std::move(manifest_builder))
      .SetID(extension_id_)
      .Build();
}

}  // namespace ash
