// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_ASH_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_ASH_H_

#include "chrome/browser/extensions/api/file_system/chrome_file_system_delegate.h"

#include <string>

#include "base/memory/scoped_refptr.h"
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
void DispatchVolumeListChangeEventAsh(content::BrowserContext* browser_context);

}  // namespace file_system_api

class ChromeFileSystemDelegateAsh : public ChromeFileSystemDelegate {
 public:
  ChromeFileSystemDelegateAsh();

  ChromeFileSystemDelegateAsh(const ChromeFileSystemDelegateAsh&) = delete;
  ChromeFileSystemDelegateAsh& operator=(const ChromeFileSystemDelegateAsh&) =
      delete;

  ~ChromeFileSystemDelegateAsh() override;

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

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_ASH_H_
