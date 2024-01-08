// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class AccessibilityExtensionLoader {
 public:
  // |manifest_filename| and |guest_manifest_filename| must outlive this
  // instance.
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

  void SetBrowserContext(content::BrowserContext* browser_context,
                         base::OnceClosure done_callback);
  void Load(content::BrowserContext* browser_context,
            base::OnceClosure done_cb);
  void Unload();

  bool loaded() { return loaded_; }

  content::BrowserContext* browser_context() { return browser_context_; }

 private:
  void LoadExtension(content::BrowserContext* browser_context,
                     base::OnceClosure done_cb);
  void ReinstallExtensionForKiosk(content::BrowserContext* profile,
                                  base::OnceClosure done_cb);
  void UnloadExtension(content::BrowserContext* browser_context);

  extensions::ExtensionId extension_id_;
  base::FilePath extension_path_;
  const base::FilePath::CharType* manifest_filename_;
  const base::FilePath::CharType* guest_manifest_filename_;
  base::RepeatingClosure unload_callback_;

  raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_ =
      nullptr;
  bool loaded_ = false;

  // Whether this extension was reset for kiosk mode.
  bool was_reset_for_kiosk_ = false;

  base::WeakPtrFactory<AccessibilityExtensionLoader> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_EXTENSION_LOADER_H_
