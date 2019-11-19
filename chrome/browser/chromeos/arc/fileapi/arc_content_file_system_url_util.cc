// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/fileapi/arc_content_file_system_url_util.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "net/base/escape.h"
#include "storage/browser/file_system/file_system_url.h"

namespace arc {

const char kContentFileSystemMountPointName[] = "arc-content";
const char kFileSystemFileproviderUrl[] =
    "content://org.chromium.arc.file_system.fileprovider/";

const base::FilePath::CharType kContentFileSystemMountPointPath[] =
    FILE_PATH_LITERAL("/special/arc-content");

std::string EscapeArcUrl(const GURL& arc_url) {
  return net::EscapeQueryParamValue(arc_url.spec(), false);
}

GURL UnescapeArcUrl(const std::string& escaped_arc_url) {
  return GURL(net::UnescapeBinaryURLComponent(escaped_arc_url));
}

GURL ArcUrlToExternalFileUrl(const GURL& arc_url) {
  // Return "externalfile:arc-content/<|arc_url| escaped>".
  base::FilePath virtual_path =
      base::FilePath::FromUTF8Unsafe(kContentFileSystemMountPointName)
          .Append(base::FilePath::FromUTF8Unsafe(EscapeArcUrl(arc_url)));
  return chromeos::VirtualPathToExternalFileURL(virtual_path);
}

GURL ExternalFileUrlToArcUrl(const GURL& external_file_url) {
  base::FilePath virtual_path =
      chromeos::ExternalFileURLToVirtualPath(external_file_url);
  base::FilePath path_after_root;
  if (!base::FilePath::FromUTF8Unsafe(kContentFileSystemMountPointName)
           .AppendRelativePath(virtual_path, &path_after_root)) {
    return GURL();
  }
  return UnescapeArcUrl(path_after_root.AsUTF8Unsafe());
}

GURL FileSystemUrlToArcUrl(const storage::FileSystemURL& url) {
  return PathToArcUrl(url.path());
}

GURL PathToArcUrl(const base::FilePath& path) {
  base::FilePath path_after_mount_point;
  if (!base::FilePath(kContentFileSystemMountPointPath)
           .AppendRelativePath(path, &path_after_mount_point)) {
    return GURL();
  }
  return UnescapeArcUrl(path_after_mount_point.AsUTF8Unsafe());
}

}  // namespace arc
