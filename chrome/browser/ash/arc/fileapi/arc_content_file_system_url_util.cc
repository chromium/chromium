// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"

#include "base/files/file_path.h"
#include "base/strings/escape.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "storage/browser/file_system/file_system_url.h"

namespace arc {

const char kContentFileSystemMountPointName[] = "arc-content";

const base::FilePath::CharType kContentFileSystemMountPointPath[] =
    FILE_PATH_LITERAL("/special/arc-content");

std::string EscapeArcUrl(const GURL& arc_url) {
  return base::EscapeQueryParamValue(arc_url.spec(), false);
}

GURL UnescapeArcUrl(const std::string& escaped_arc_url) {
  return GURL(base::UnescapeBinaryURLComponent(escaped_arc_url));
}

GURL ArcUrlToExternalFileUrl(const GURL& arc_url) {
  // Return "externalfile:arc-content/<|arc_url| escaped>".
  base::FilePath virtual_path =
      base::FilePath::FromASCII(kContentFileSystemMountPointName)
          .Append(base::FilePath::FromUTF8Unsafe(EscapeArcUrl(arc_url)));
  return ash::VirtualPathToExternalFileURL(virtual_path);
}

GURL ExternalFileUrlToArcUrl(const GURL& external_file_url) {
  base::FilePath virtual_path =
      ash::ExternalFileURLToVirtualPath(external_file_url);
  base::FilePath path_after_root;
  if (!base::FilePath::FromASCII(kContentFileSystemMountPointName)
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
