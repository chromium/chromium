// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/external_file_url_util.h"

#include <string>
#include <vector>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fusebox/fusebox_server.h"
#include "chrome/browser/extensions/extension_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace ash {

bool IsExternalFileURLType(storage::FileSystemType type, bool force) {
  return type == storage::kFileSystemTypeDeviceMediaAsFileStorage ||
         type == storage::kFileSystemTypeProvided ||
         type == storage::kFileSystemTypeArcContent ||
         type == storage::kFileSystemTypeArcDocumentsProvider || force;
}

GURL FileSystemURLToExternalFileURL(
    const storage::FileSystemURL& file_system_url,
    bool force) {
  if (file_system_url.mount_type() != storage::kFileSystemTypeExternal ||
      !IsExternalFileURLType(file_system_url.type(), force)) {
    return GURL();
  }

  return VirtualPathToExternalFileURL(file_system_url.virtual_path());
}

base::FilePath ExternalFileURLToVirtualPath(const GURL& url) {
  if (!url.is_valid() || url.scheme() != content::kExternalFileScheme)
    return base::FilePath();
  return base::FilePath::FromUTF8Unsafe(
      base::UnescapeBinaryURLComponent(url.path_piece()));
}

GURL VirtualPathToExternalFileURL(const base::FilePath& virtual_path) {
  return GURL(base::StringPrintf(
      "%s:%s", content::kExternalFileScheme,
      base::EscapePath(virtual_path.AsUTF8Unsafe()).c_str()));
}

GURL CreateExternalFileURLFromPath(content::BrowserContext* browser_context,
                                   const base::FilePath& path,
                                   bool force) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL raw_file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          browser_context, path, file_manager::util::GetFileManagerURL(),
          &raw_file_system_url)) {
    return GURL();
  }

  const storage::FileSystemURL file_system_url =
      file_manager::util::GetFileManagerFileSystemContext(browser_context)
          ->CrackURLInFirstPartyContext(raw_file_system_url);
  if (!file_system_url.is_valid())
    return GURL();

  return FileSystemURLToExternalFileURL(file_system_url, force);
}

GURL ExternalFileURLToFuseboxMonikerFileURL(
    content::BrowserContext* browser_context,
    const GURL& url,
    bool read_only,
    base::TimeDelta lifetime) {
  const base::FilePath virtual_path = ExternalFileURLToVirtualPath(url);

  const storage::FileSystemURL fs_url =
      file_manager::util::GetFileManagerFileSystemContext(browser_context)
          ->CreateCrackedFileSystemURL(
              blink::StorageKey::CreateFirstParty(
                  file_manager::util::GetFilesAppOrigin()),
              storage::kFileSystemTypeExternal, virtual_path);
  if (!fs_url.is_valid()) {
    return GURL();
  }

  fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
  if (!fusebox_server) {
    return GURL();
  }

  fusebox::Moniker moniker = fusebox_server->CreateMoniker(fs_url, read_only);

  // Keep the Moniker alive for the lifetime. We could be cleverer about
  // scheduling the clean up, but "destroy after a fixed amount of time" is
  // simple and works well enough in practice.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](fusebox::Moniker moniker) {
            fusebox::Server* fusebox_server = fusebox::Server::GetInstance();
            if (fusebox_server) {
              fusebox_server->DestroyMoniker(moniker);
            }
          },
          moniker),
      lifetime);

  base::FilePath moniker_path(fusebox::MonikerMap::GetFilename(moniker));
  return net::FilePathToFileURL(
      moniker_path.AddExtension(virtual_path.Extension()));
}

}  // namespace ash
