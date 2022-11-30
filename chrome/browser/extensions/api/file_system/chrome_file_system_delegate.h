// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_

#include "extensions/browser/api/file_system/file_system_delegate.h"

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_function.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "extensions/browser/api/file_system/consent_provider.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS)
namespace file_system_api {

extern const char kConsentImpossible[];
extern const char kNotSupportedOnNonKioskSessionError[];
extern const char kRequiresFileSystemWriteError[];
extern const char kSecurityError[];
extern const char kVolumeNotFoundError[];

// Returns error message, or null if none.
const char* ConsentResultToError(ConsentProvider::Consent result);

}  // namespace file_system_api
#endif  // BUILDFLAG(IS_CHROMEOS)

class ChromeFileSystemDelegate : public FileSystemDelegate {
 public:
  ChromeFileSystemDelegate();

  ChromeFileSystemDelegate(const ChromeFileSystemDelegate&) = delete;
  ChromeFileSystemDelegate& operator=(const ChromeFileSystemDelegate&) = delete;

  ~ChromeFileSystemDelegate() override;

  // FileSystemDelegate:
  base::FilePath GetDefaultDirectory() override;
  base::FilePath GetManagedSaveAsDirectory(
      content::BrowserContext* browser_context,
      const Extension& extension) override;
  bool ShowSelectFileDialog(
      scoped_refptr<ExtensionFunction> extension_function,
      ui::SelectFileDialog::Type type,
      const base::FilePath& default_path,
      const ui::SelectFileDialog::FileTypeInfo* file_types,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback) override;
  void ConfirmSensitiveDirectoryAccess(bool has_write_permission,
                                       const std::u16string& app_name,
                                       content::WebContents* web_contents,
                                       base::OnceClosure on_accept,
                                       base::OnceClosure on_cancel) override;
  int GetDescriptionIdForAcceptType(const std::string& accept_type) override;
#if BUILDFLAG(IS_CHROMEOS)
  // |consent_provider| must at least live as long as |requester|.
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
#endif  // BUILDFLAG(IS_CHROMEOS)
  SavedFilesServiceInterface* GetSavedFilesService(
      content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_CHROME_FILE_SYSTEM_DELEGATE_H_
