// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_

#include <memory>

#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class ChromeComponentExtensionResourceManager
    : public ComponentExtensionResourceManager {
 public:
  ChromeComponentExtensionResourceManager();

  ChromeComponentExtensionResourceManager(
      const ChromeComponentExtensionResourceManager&) = delete;
  ChromeComponentExtensionResourceManager& operator=(
      const ChromeComponentExtensionResourceManager&) = delete;

  ~ChromeComponentExtensionResourceManager() override;

  // Overridden from ComponentExtensionResourceManager:
  bool IsComponentExtensionResource(const base::FilePath& extension_path,
                                    const base::FilePath& resource_path,
                                    int* resource_id) const override;
  const ui::TemplateReplacements* GetTemplateReplacementsForExtension(
      const ExtensionId& extension_id) const override;

 private:
  class Data;

  void LazyInitData() const;

  // Logically const. Initialized on demand to keep browser start-up fast.
  mutable std::unique_ptr<const Data> data_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_COMPONENT_EXTENSION_RESOURCE_MANAGER_H_
