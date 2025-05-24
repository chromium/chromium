// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_DESKTOP_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_DESKTOP_H_

#include "chrome/browser/extensions/api/developer_private/extension_info_generator_shared.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// Generates the developerPrivate api's specification for ExtensionInfo.
// This class is designed to only have one generation running at a time!
//
// This class is used on desktop OSes except Android. See comments in
// ExtensionInfoGeneratorShared how it is organized.
class ExtensionInfoGenerator : public ExtensionInfoGeneratorShared {
 public:
  explicit ExtensionInfoGenerator(content::BrowserContext* context);

  ExtensionInfoGenerator(const ExtensionInfoGenerator&) = delete;
  ExtensionInfoGenerator& operator=(const ExtensionInfoGenerator&) = delete;

  ~ExtensionInfoGenerator() override;

 protected:
  // Fills an ExtensionInfo for the given `extension` and `state`, and
  // asynchronously adds it to the `list`.
  void FillExtensionInfo(const Extension& extension,
                         api::developer_private::ExtensionState state,
                         api::developer_private::ExtensionInfo info) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_DESKTOP_H_
