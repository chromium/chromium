// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace chromeos {

class AccessibilityExtensionLoader {
 public:
  AccessibilityExtensionLoader(const std::string& extension_id,
                               const base::FilePath& extension_path,
                               const base::Closure& unload_callback);
  ~AccessibilityExtensionLoader();

  void SetProfile(Profile* profile, const base::Closure& done_callback);
  void Load(Profile* profile, const base::Closure& done_cb);
  void Unload();
  void LoadExtension(Profile* profile, base::Closure done_cb);

 private:
  void UnloadExtensionFromProfile(Profile* profile);

  Profile* profile_;
  std::string extension_id_;
  base::FilePath extension_path_;

  bool loaded_;

  base::Closure unload_callback_;

  base::WeakPtrFactory<AccessibilityExtensionLoader> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AccessibilityExtensionLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
