// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_EXTENSION_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_EXTENSION_BUILDER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"

namespace extensions {
class Extension;
}

namespace ash {

// Wrapper around extensions::ExtensionBuilder for creating extension::Extension
// instances for usage in kiosk app tests.
// TODO(b/227985497): Turn this into a proper builder
class TestKioskExtensionBuilder {
 public:
  TestKioskExtensionBuilder(extensions::Manifest::Type type,
                            const extensions::ExtensionId& extension_id);
  TestKioskExtensionBuilder(const TestKioskExtensionBuilder&) = delete;
  TestKioskExtensionBuilder& operator=(const TestKioskExtensionBuilder&) =
      delete;
  TestKioskExtensionBuilder(TestKioskExtensionBuilder&&);
  ~TestKioskExtensionBuilder();

  const extensions::ExtensionId& extension_id() const { return extension_id_; }
  const std::string& version() const { return version_; }

  TestKioskExtensionBuilder& set_kiosk_enabled(bool enabled) {
    kiosk_enabled_ = enabled;
    return *this;
  }

  TestKioskExtensionBuilder& set_offline_enabled(bool enabled) {
    offline_enabled_ = enabled;
    return *this;
  }

  TestKioskExtensionBuilder& set_version(const std::string& version) {
    version_ = version;
    return *this;
  }

  TestKioskExtensionBuilder& AddSecondaryExtension(const std::string& id);
  TestKioskExtensionBuilder& AddSecondaryExtensionWithEnabledOnLaunch(
      const std::string& id,
      bool enabled_on_launch);

  scoped_refptr<const extensions::Extension> Build() const;

 private:
  const extensions::Manifest::Type type_;
  const extensions::ExtensionId extension_id_;

  bool kiosk_enabled_ = true;
  bool offline_enabled_ = true;
  std::vector<extensions::SecondaryKioskAppInfo> secondary_extensions_;
  std::string version_ = "1.0";
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_KIOSK_EXTENSION_BUILDER_H_
