// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_LACROS_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_LACROS_H_

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "extensions/browser/extension_function.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class ConsentProvider;
class Extension;

namespace file_system_api {

// Dispatches an event about a mounted or unmounted volume in the system to
// each extension which can request it.
void DispatchVolumeListChangeEventLacros(
    content::BrowserContext* browser_context,
    const std::vector<crosapi::mojom::VolumePtr>& volume_list);

}  // namespace file_system_api

class ChromeFileSystemDelegateLacros : public ChromeFileSystemDelegate {
 public:
  ChromeFileSystemDelegateLacros();

  ChromeFileSystemDelegateLacros(const ChromeFileSystemDelegateLacros&) =
      delete;
  ChromeFileSystemDelegateLacros& operator=(
      const ChromeFileSystemDelegateLacros&) = delete;

  ~ChromeFileSystemDelegateLacros() override;

  // ChromeFileSystemDelegate:
  void RequestFileSystem(content::BrowserContext* browser_context,
                         scoped_refptr<ExtensionFunction> requester,
                         ConsentProvider* consent_provider,
                         const Extension& extension,
                         std::string volume_id,
                         bool writable,
                         FileSystemCallback success_callback,
                         ErrorCallback error_callback) override;
  void GetVolumeList(content::BrowserContext* browser_context,
                     VolumeListCallback success_callback,
                     ErrorCallback error_callback) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_LACROS_H_
