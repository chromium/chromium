// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides File API related utilities.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILEAPI_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILEAPI_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace storage {
class FileSystemContext;
}

namespace ui {
struct SelectedFileInfo;
}

namespace file_manager {
namespace util {

// Structure information necessary to create a EntryDefinition, and therefore
// an Entry object on the JavaScript side.
struct FileDefinition {
  base::FilePath virtual_path;
  base::FilePath absolute_path;
  bool is_directory = false;
};

// Contains all information needed to create an Entry object in custom bindings.
struct EntryDefinition {
  EntryDefinition();
  EntryDefinition(const EntryDefinition& other);
  ~EntryDefinition();

  std::string file_system_root_url;  // Used to create DOMFileSystem.
  std::string file_system_name;      // Value of DOMFileSystem.name.
  base::FilePath full_path;          // Value of Entry.fullPath.
  // Whether to create FileEntry or DirectoryEntry when the corresponding entry
  // is not found.
  bool is_directory = false;
  base::File::Error error = base::File::FILE_ERROR_FAILED;
};

typedef std::vector<FileDefinition> FileDefinitionList;
typedef std::vector<EntryDefinition> EntryDefinitionList;
typedef std::vector<ui::SelectedFileInfo> SelectedFileInfoList;
typedef std::vector<blink::mojom::FileChooserFileInfoPtr>
    FileChooserFileInfoList;

// The callback used by ConvertFileDefinitionToEntryDefinition. Returns the
// result of the conversion.
typedef base::OnceCallback<void(const EntryDefinition& entry_definition)>
    EntryDefinitionCallback;

// The callback used by ConvertFileDefinitionListToEntryDefinitionList. Returns
// the result of the conversion as a list.
typedef base::OnceCallback<void(
    std::unique_ptr<EntryDefinitionList> entry_definition_list)>
    EntryDefinitionListCallback;

// The callback used by
// ConvertFileSelectedInfoListToFileChooserFileInfoList. Returns the result of
// the conversion as a list.
typedef base::OnceCallback<void(FileChooserFileInfoList)>
    FileChooserFileInfoListCallback;

// Returns the URL of the system File Manager. If you think you need to use File
// Manager ID or URL, use this function instead. This function guarantees that
// the correct and current URL is returned. If you need to access just the ID
// of the system File Manager, call host() method on the returned URL.
// TODO(crbug.com/40752851): Replace with dynamic listener URL.
const GURL GetFileManagerURL();

// Returns whether the given URL identifies the File Manager as a source. This
// can be used to see if a private API calls come from the File Manager or not.
bool IsFileManagerURL(const GURL& source_url);

// Returns the default file system context for Files app. This is a convenience
// method that should be used only if you are ABSOLUTELY CERTAIN that you are
// performing some functions on the behalf of the Files app yet your code does
// not readily have access to the system File Manager ID or URL.
storage::FileSystemContext* GetFileManagerFileSystemContext(
    content::BrowserContext* browser_context);

// Returns a file system context associated with the given browser_context and
// the source URL. The source URL is the URL that identifies the application,
// such as chrome-extension://<extension-id>/ or chrome://<app-id>/. In private
// APIs it is available as source_url(). You can also use GetFileManagerURL
// with this call.
storage::FileSystemContext* GetFileSystemContextForSourceURL(
    content::BrowserContext* browser_context,
    const GURL& source_url);

// Returns a file system context associated with the given browser_context and
// the render view host.
storage::FileSystemContext* GetFileSystemContextForRenderFrameHost(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* render_frame_host);

// Converts AbsolutePath (e.g., "/media/removable/foo" or
// "/home/chronos/u-xxx/Downloads") into filesystem URL. Returns false
// if |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToFileSystemUrl(
    content::BrowserContext* browser_context,
    const base::FilePath& absolute_path,
    const GURL& source_url,
    GURL* url);

// Converts AbsolutePath into RelativeFileSystemPath (e.g.,
// "/media/removable/foo/bar" => "removable/foo/bar".) Returns false if
// |absolute_path| is not managed by the external filesystem provider.
bool ConvertAbsoluteFilePathToRelativeFileSystemPath(
    content::BrowserContext* browser_context,
    const GURL& source_url,
    const base::FilePath& absolute_path,
    base::FilePath* relative_path);

// Converts a file definition to a entry definition and returns the result
// via a callback. Must be called on UI thread.
void ConvertFileDefinitionToEntryDefinition(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinition& file_definition,
    EntryDefinitionCallback callback);

// Converts a list of file definitions into a list of entry definitions and
// returns it via |callback|. The method is safe, |file_definition_list| is
// copied internally. The output list has the same order of items and size as
// the input vector. Must be called on UI thread.
void ConvertFileDefinitionListToEntryDefinitionList(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const url::Origin& origin,
    const FileDefinitionList& file_definition_list,
    EntryDefinitionListCallback callback);

// Converts SelectedFileInfoList into FileChooserFileInfoList.
void ConvertSelectedFileInfoListToFileChooserFileInfoList(
    storage::FileSystemContext* context,
    const url::Origin& origin,
    const SelectedFileInfoList& selected_info_list,
    FileChooserFileInfoListCallback callback);

// Converts EntryDefinition to something File API stack can understand.
base::Value::Dict ConvertEntryDefinitionToValue(
    const EntryDefinition& entry_definition);

base::Value::List ConvertEntryDefinitionListToListValue(
    const EntryDefinitionList& entry_definition_list);

// Checks if a directory exists at |directory_path| absolute path.
void CheckIfDirectoryExists(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& directory_path,
    storage::FileSystemOperationRunner::StatusCallback callback);

// Get metadata for an entry at |entry_path| absolute path.
void GetMetadataForPath(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath& entry_path,
    storage::FileSystemOperationRunner::GetMetadataFieldSet fields,
    storage::FileSystemOperationRunner::GetMetadataCallback callback);

// Groups a FileSystemURL and a related ScopedFSHandle.
//
// The URL is guaranteed to be valid as long as the handle is valid.
struct FileSystemURLAndHandle {
  storage::FileSystemURL url;
  storage::IsolatedContext::ScopedFSHandle handle;
};

// Obtains isolated file system URL from |virtual_path| pointing a file in the
// external file system.
FileSystemURLAndHandle CreateIsolatedURLFromVirtualPath(
    const storage::FileSystemContext& context,
    const url::Origin& origin,
    const base::FilePath& virtual_path);

// Given a |destination_folder| and a |filename|, returns a suitable path inside
// folder that does not already exist. First it checks whether |filename| exists
// inside |destination_folder|. If it does, it adds a parenthesised number (e.g.
// " (1)" before the extension to deduplicate the filename.
void GenerateUnusedFilename(
    storage::FileSystemURL destination_folder,
    base::FilePath filename,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::OnceCallback<void(base::FileErrorOr<storage::FileSystemURL>)>
        callback);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILEAPI_UTIL_H_
