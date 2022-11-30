// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace ash {

class AccessibilityExtensionLoader {
 public:
  AccessibilityExtensionLoader(
      const std::string& extension_id,
      const base::FilePath& extension_path,
      const base::FilePath::CharType* manifest_filename,
      const base::FilePath::CharType* guest_manifest_filename,
      base::RepeatingClosure unload_callback);

  AccessibilityExtensionLoader(const AccessibilityExtensionLoader&) = delete;
  AccessibilityExtensionLoader& operator=(const AccessibilityExtensionLoader&) =
      delete;

  ~AccessibilityExtensionLoader();

  void SetProfile(Profile* profile, base::OnceClosure done_callback);
  void Load(Profile* profile, base::OnceClosure done_cb);
  void Unload();

  bool loaded() { return loaded_; }

  Profile* profile() { return profile_; }

 private:
  void LoadExtension(Profile* profile, base::OnceClosure done_cb);
  void LoadExtensionImpl(Profile* profile, base::OnceClosure done_cb);
  void ReinstallExtensionForKiosk(Profile* profile, base::OnceClosure done_cb);
  void UnloadExtensionFromProfile(Profile* profile);

  Profile* profile_;
  std::string extension_id_;
  base::FilePath extension_path_;

  const base::FilePath::CharType* manifest_filename_ = nullptr;

  const base::FilePath::CharType* guest_manifest_filename_ = nullptr;

  bool loaded_;

  // Whether this extension was reset for kiosk mode.
  bool was_reset_for_kiosk_ = false;

  base::RepeatingClosure unload_callback_;

  base::WeakPtrFactory<AccessibilityExtensionLoader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
