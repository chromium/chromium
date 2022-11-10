// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/external_file_url_util.h"

#include <string>
#include <vector>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
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

GURL CreateExternalFileURLFromPath(Profile* profile,
                                   const base::FilePath& path,
                                   bool force) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL raw_file_system_url;
  if (!file_manager::util::ConvertAbsoluteFilePathToFileSystemUrl(
          profile, path, file_manager::util::GetFileManagerURL(),
          &raw_file_system_url)) {
    return GURL();
  }

  const storage::FileSystemURL file_system_url =
      file_manager::util::GetFileManagerFileSystemContext(profile)
          ->CrackURLInFirstPartyContext(raw_file_system_url);
  if (!file_system_url.is_valid())
    return GURL();

  return FileSystemURLToExternalFileURL(file_system_url, force);
}

}  // namespace ash
