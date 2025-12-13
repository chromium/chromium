// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_storage.h"

#include <vector>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using content::BrowserThread;
using content::RenderViewHost;
using content::WebContents;
using storage::IsolatedContext;

namespace {

static const char kRootName[] = "<root>";

IsolatedContext* isolated_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  IsolatedContext* isolated_context = IsolatedContext::GetInstance();
  DCHECK(isolated_context);
  return isolated_context;
}

}  // namespace

DevToolsFileStorage::DevToolsFileStorage(WebContents* web_contents)
    : web_contents_(web_contents) {}

DevToolsFileStorage::~DevToolsFileStorage() = default;

DevToolsFileHelper::FileSystem DevToolsFileStorage::RegisterFileSystem(
    const base::FilePath& path,
    const std::string& type) {
  CHECK(web_contents_->GetLastCommittedURL().SchemeIs(
      content::kChromeDevToolsScheme));
  std::string root_name(kRootName);
  auto file_system = isolated_context()->RegisterFileSystemForPath(
      storage::kFileSystemTypeLocal, std::string(), path, &root_name);

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  RenderViewHost* render_view_host =
      web_contents_->GetPrimaryMainFrame()->GetRenderViewHost();
  int renderer_id = render_view_host->GetProcess()->GetDeprecatedID();
  policy->GrantReadFileSystem(renderer_id, file_system.id());
  policy->GrantWriteFileSystem(renderer_id, file_system.id());
  policy->GrantCreateFileForFileSystem(renderer_id, file_system.id());
  policy->GrantDeleteFromFileSystem(renderer_id, file_system.id());

  // We only need file level access for reading FileEntries. Saving FileEntries
  // just needs the file system to have read/write access, which is granted
  // above if required.
  if (!policy->CanReadFile(renderer_id, path)) {
    policy->GrantReadFile(renderer_id, path);
  }
  GURL origin = web_contents_->GetLastCommittedURL().DeprecatedGetOriginAsURL();
  std::string file_system_name =
      storage::GetIsolatedFileSystemName(origin, file_system.id());
  std::string root_url = storage::GetIsolatedFileSystemRootURIString(
      origin, file_system.id(), root_name);
  return DevToolsFileHelper::FileSystem(type, file_system_name, root_url,
                                        path.AsUTF8Unsafe());
}

void DevToolsFileStorage::UnregisterFileSystem(const base::FilePath& path) {
  isolated_context()->RevokeFileSystemByPath(path);
}

std::vector<base::FilePath> DevToolsFileStorage::GetDraggedFileSystemPaths(
    const GURL& file_system_url) {
  auto root_url = isolated_context()->CrackURL(
      file_system_url, blink::StorageKey::CreateFirstParty(
                           url::Origin::Create(file_system_url)));
  std::vector<base::FilePath> file_system_paths;
  if (root_url.is_valid() && root_url.path().empty()) {
    std::vector<storage::MountPoints::MountPointInfo> mount_points;
    isolated_context()->GetDraggedFileInfo(root_url.filesystem_id(),
                                           &mount_points);
    for (const auto& mount_point : mount_points) {
      file_system_paths.push_back(mount_point.path);
    }
  }
  return file_system_paths;
}
