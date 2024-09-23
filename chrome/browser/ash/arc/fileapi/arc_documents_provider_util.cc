// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_documents_provider_util.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace arc {

namespace {

struct MimeTypeToExtensions {
  const char* mime_type;
  const char* extensions;
};

// The mapping from MIME types to file name extensions, taken from Android T.
// See: frameworks/base/media/java/android/media/MediaFile.java
constexpr MimeTypeToExtensions kAndroidMimeTypeMappings[] = {
    {"application/vnd.ms-powerpoint", "ppt,pot,pps,ppa"},
    {"application/msword", "doc,dot"},
    {"application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "pptx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
     "xlsx"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "docx"},
    {"application/ogg", "ogg,oga"},
    {"application/pdf", "pdf"},
    {"application/vnd.apple.mpegurl", "m3u8"},
    {"application/vnd.ms-excel", "xls,xlt,xla"},
    {"application/vnd.ms-wpl", "wpl"},
    {"application/x-android-drm-fl", "fl"},
    {"application/x-mpegurl", "m3u"},
    {"application/zip", "zip"},
    {"audio/aac", "aac"},
    {"audio/aac-adts", "aac"},
    {"audio/amr", "amr"},
    {"audio/amr-wb", "awb"},
    {"audio/flac", "flac"},
    {"audio/imelody", "imy"},
    {"audio/midi", "mid,midi,xmf,rtttl,rtx,ota,mxmf"},
    {"audio/mp4", "m4a"},
    {"audio/mpeg", "mp3,mpga"},
    {"audio/mpegurl", "m3u8"},
    {"audio/ogg", "ogg"},
    {"audio/sp-midi", "smf"},
    {"audio/x-aiff", "aiff"},
    {"audio/x-matroska", "mka"},
    {"audio/x-mpegurl", "m3u,m3u8"},
    {"audio/x-ms-wma", "wma"},
    {"audio/x-scpls", "pls"},
    {"audio/x-wav", "wav"},
    {"image/gif", "gif"},
    {"image/jp2", "jpg2"},
    {"image/jpeg", "jpg,jpeg"},
    {"image/jpx", "jpx"},
    {"image/png", "png"},
    {"image/vnd.wap.wbmp", "wbmp"},
    {"image/webp", "webp"},
    {"image/x-adobe-dng", "dng"},
    {"image/x-canon-cr2", "cr2"},
    {"image/x-fuji-raf", "raf"},
    {"image/x-heif", "heif"},
    {"image/x-ms-bmp", "bmp"},
    {"image/x-nikon-nef", "nef"},
    {"image/x-nikon-nrw", "nrw"},
    {"image/x-olympus-orf", "orf"},
    {"image/x-panasonic-rw2", "rw2"},
    {"image/x-pentax-pef", "pef"},
    {"image/x-samsung-srw", "srw"},
    {"image/x-sony-arw", "arw"},
    {"text/html", "html,htm"},
    {"text/plain", "txt"},
    {"video/3gpp", "3gp,3gpp"},
    {"video/3gpp2", "3g2,3gpp2"},
    {"video/avi", "avi"},
    {"video/mp2p", "mpg,mpeg"},
    {"video/mp2ts", "ts"},
    {"video/mp4", "mp4,m4v"},
    {"video/mpeg", "mpg,mpeg"},
    {"video/quicktime", "mov"},
    {"video/webm", "webm"},
    {"video/x-matroska", "mkv"},
    {"video/x-ms-asf", "asf"},
    {"video/x-ms-wmv", "wmv"},
};

constexpr char kApplicationOctetStreamMimeType[] = "application/octet-stream";
constexpr char kDocumentsProviderVolumeIdPrefix[] = "documents_provider:";

}  // namespace

// This is based on net/base/escape.cc: net::(anonymous namespace)::Escape.
// TODO(nya): Consider consolidating this function with EscapeFileSystemId() in
// chrome/browser/ash/file_system_provider/mount_path_util.cc.
// This version differs from the other one in the point that dots are not always
// escaped because authorities often contain harmless dots.
std::string EscapePathComponent(const std::string& name) {
  std::string escaped;
  // Escape dots only when they forms a special file name.
  if (name == "." || name == "..") {
    base::ReplaceChars(name, ".", "%2E", &escaped);
    return escaped;
  }
  // Escape % and / only.
  for (size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if (c == '%' || c == '/')
      base::StringAppendF(&escaped, "%%%02X", c);
    else
      escaped.push_back(c);
  }
  return escaped;
}

std::string UnescapePathComponent(const std::string& escaped) {
  return base::UnescapeURLComponent(
      escaped,
      base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
}

const char kDocumentsProviderMountPointName[] = "arc-documents-provider";
const base::FilePath::CharType kDocumentsProviderMountPointPath[] =
    "/special/arc-documents-provider";
const char kAndroidDirectoryMimeType[] = "vnd.android.document/directory";

base::FilePath GetDocumentsProviderMountPath(const std::string& authority,
                                             const std::string& root_id) {
  return base::FilePath(kDocumentsProviderMountPointPath)
      .Append(GetDocumentsProviderMountPathSuffix(authority, root_id));
}

base::FilePath GetDocumentsProviderMountPathSuffix(const std::string& authority,
                                                   const std::string& root_id) {
  return base::FilePath(EscapePathComponent(authority))
      .Append(EscapePathComponent(root_id));
}

bool ParseDocumentsProviderPath(const base::FilePath& path,
                                std::string* authority,
                                std::string* root_id) {
  if (!base::FilePath(kDocumentsProviderMountPointPath).IsParent(path))
    return false;

  // Filesystem path format for documents provider is:
  // /special/arc-documents-provider/<authority>/<root_id>/<relative_path>
  std::vector<base::FilePath::StringType> components = path.GetComponents();
  if (components.size() < 5)
    return false;

  *authority = UnescapePathComponent(components[3]);
  *root_id = UnescapePathComponent(components[4]);
  return true;
}

bool ParseDocumentsProviderUrl(const storage::FileSystemURL& url,
                               std::string* authority,
                               std::string* root_id,
                               base::FilePath* path) {
  if (url.type() != storage::kFileSystemTypeArcDocumentsProvider)
    return false;
  base::FilePath url_path_stripped = url.path().StripTrailingSeparators();

  if (!ParseDocumentsProviderPath(url_path_stripped, authority, root_id)) {
    return false;
  }

  base::FilePath root_path =
      GetDocumentsProviderMountPath(*authority, *root_id);
  // Special case: AppendRelativePath() fails for identical paths.
  if (url_path_stripped == root_path) {
    path->clear();
  } else {
    bool success = root_path.AppendRelativePath(url_path_stripped, path);
    DCHECK(success);
  }
  return true;
}

GURL BuildDocumentUrl(const std::string& authority,
                      const std::string& document_id) {
  return GURL(base::StringPrintf(
      "content://%s/document/%s",
      base::EscapeQueryParamValue(authority, false /* use_plus */).c_str(),
      base::EscapeQueryParamValue(document_id, false /* use_plus */).c_str()));
}

std::vector<base::FilePath::StringType> GetExtensionsForArcMimeType(
    const std::string& mime_type) {
  // net::GetExtensionsForMimeType() returns unwanted extensions like
  // "exe,com,bin" for application/octet-stream.
  if (net::MatchesMimeType(kApplicationOctetStreamMimeType, mime_type))
    return std::vector<base::FilePath::StringType>();

  // Attempt net::GetExtensionsForMimeType().
  {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType(mime_type, &extensions);
    if (!extensions.empty()) {
      base::FilePath::StringType preferred_extension;
      if (net::GetPreferredExtensionForMimeType(mime_type,
                                                &preferred_extension)) {
        auto iter = base::ranges::find(extensions, preferred_extension);
        if (iter == extensions.end()) {
          // This is unlikely to happen, but there is no guarantee.
          extensions.insert(extensions.begin(), preferred_extension);
        } else {
          std::swap(extensions.front(), *iter);
        }
      }
      return extensions;
    }
  }

  // Fallback to our hard-coded list.
  for (const auto& entry : kAndroidMimeTypeMappings) {
    if (net::MatchesMimeType(entry.mime_type, mime_type)) {
      // We assume base::FilePath::StringType == std::string as this code is
      // built only on Chrome OS.
      return base::SplitString(entry.extensions, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_ALL);
    }
  }

  return std::vector<base::FilePath::StringType>();
}

std::string StripMimeSubType(const std::string& mime_type) {
  if (mime_type.empty())
    return mime_type;
  size_t index = mime_type.find_first_of('/', 0);
  if (index == 0 || index == mime_type.size() - 1 ||
      index == std::string::npos) {
    // This looks malformed, return an empty string.
    return std::string();
  }
  return mime_type.substr(0, index);
}

// This is based on net/base/mime_util.cc: net::FindMimeType.
std::string FindArcMimeTypeFromExtension(const std::string& ext) {
  for (const auto& mapping : kAndroidMimeTypeMappings) {
    std::vector<std::string_view> extensions = base::SplitStringPiece(
        mapping.extensions, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (base::Contains(extensions, ext))
      return mapping.mime_type;
  }
  return std::string();
}

// TODO(crbug.com/40498938): Consolidate with the similar logic for Drive.
base::FilePath::StringType GetFileNameForDocument(
    const mojom::DocumentPtr& document) {
  base::FilePath::StringType filename = document->display_name;

  // Replace path separators appearing in the file name.
  // Chrome OS is POSIX and kSeparators is "/".
  base::ReplaceChars(filename, base::FilePath::kSeparators, "_", &filename);

  // Do not allow an empty file name and all-dots file names.
  if (filename.empty() ||
      filename.find_first_not_of('.', 0) == std::string::npos) {
    filename = "_";
  }

  // Since Chrome detects MIME type from file name extensions, we need to change
  // the file name extension of the document if it does not match with its MIME
  // type.
  // For example, Audio Media Provider presents a music file with its title as
  // the file name.
  base::FilePath::StringType extension =
      base::ToLowerASCII(base::FilePath(filename).Extension());
  if (!extension.empty())
    extension = extension.substr(1);  // Strip the leading dot.
  std::vector<base::FilePath::StringType> possible_extensions =
      GetExtensionsForArcMimeType(document->mime_type);

  if (!possible_extensions.empty() &&
      !base::Contains(possible_extensions, extension)) {
    // Lookup the extension in the hardcoded map before appending an extension,
    // as some extensions (eg. 3gp) are typed differently by Android. Only
    // append the suggested extension if the lookup fails (i.e. no valid mime
    // type returned), or the returned mime type is of a different category.
    // TODO(crbug.com/878221): Fix discrepancy in MIME types and extensions
    // between the hard coded map and the Android content provider.
    std::string missed_possible_mime_type =
        FindArcMimeTypeFromExtension(extension);
    if (missed_possible_mime_type.empty() ||
        StripMimeSubType(document->mime_type) !=
            StripMimeSubType(missed_possible_mime_type)) {
      filename =
          base::FilePath(filename).AddExtension(possible_extensions[0]).value();
    }
  }

  return filename;
}

std::string GetDocumentsProviderVolumeId(const std::string& authority,
                                         const std::string& root_id) {
  // Since |authority| can not have '/', a pair of |authority| and |root_id| is
  // guaranteed to result in a unique volume id.
  return kDocumentsProviderVolumeIdPrefix + authority + "/" + root_id;
}

}  // namespace arc
