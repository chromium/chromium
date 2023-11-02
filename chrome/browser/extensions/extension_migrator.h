// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MIGRATOR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MIGRATOR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/external_loader.h"

class Profile;

namespace extensions {

// An ExternalLoader that provides the new extension for the following
// scenarios:
//   - Existing profile that has the old.
//   - Existing profile that already has the new.
// Note that the old extension is not removed.
class ExtensionMigrator : public ExternalLoader {
 public:
  ExtensionMigrator(Profile* profile,
                    const std::string& old_id,
                    const std::string& new_id);

  ExtensionMigrator(const ExtensionMigrator&) = delete;
  ExtensionMigrator& operator=(const ExtensionMigrator&) = delete;

 protected:
  ~ExtensionMigrator() override;

  // ExternalLoader:
  void StartLoading() override;

 private:
  bool IsAppPresent(const std::string& app_id);

  raw_ptr<Profile> profile_;
  const std::string old_id_;
  const std::string new_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MIGRATOR_H_
