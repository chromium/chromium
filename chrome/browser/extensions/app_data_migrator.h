// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_DATA_MIGRATOR_H_
#define CHROME_BROWSER_EXTENSIONS_APP_DATA_MIGRATOR_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;

// This class migrates legacy packaged app data in the general storage
// partition to an isolated storage partition. This happens when a legacy
// packaged app is upgraded to a platform app. See http://crbug.com/302577
class AppDataMigrator {
 public:
  AppDataMigrator(Profile* profile, ExtensionRegistry* registry);
  ~AppDataMigrator();

  static bool NeedsMigration(const Extension* old, const Extension* extension);

  void DoMigrationAndReply(const Extension* old,
                           const Extension* extension,
                           const base::Closure& reply);

 private:
  Profile* profile_;
  ExtensionRegistry* registry_;
  base::WeakPtrFactory<AppDataMigrator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppDataMigrator);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_DATA_MIGRATOR_H_
