// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/microsoft_modules_helper.h"

#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/mime_util.h"

namespace {

const char kBaseIconUrl[] =
    "https://res.cdn.office.net/files/fabric-cdn-prod_20240925.001/assets/"
    "item-types/16/";

// The following are used to create file icon urls.
constexpr char kAudioIconPartialPath[] = "audio";
constexpr char kImagesIconPartialPath[] = "photo";
constexpr char kVideoIconPartialPath[] = "video";
constexpr char kCodeIconPartialPath[] = "code";
constexpr char kVectorIconPartialPath[] = "vector";
constexpr char kXmlDocumentIconPartialPath[] = "docx";
constexpr char kXmlPresentationIconPartialPath[] = "pptx";
constexpr char kXmlSpreadsheetIconPartialPath[] = "xlsx";
constexpr char kPlainTextIconPartialPath[] = "txt";
constexpr char kCsvIconPartialPath[] = "csv";
constexpr char kPdfIconPartialPath[] = "pdf";
constexpr char kRichTextPartialPath[] = "rtf";
constexpr char kZipPartialPath[] = "zip";
constexpr char kXmlPartialPath[] = "xml";

}  // namespace

namespace microsoft_modules_helper {

std::string GetFileExtension(std::string mime_type) {
  base::FilePath::StringType extension;
  net::GetPreferredExtensionForMimeType(mime_type, &extension);
  std::string result;

#if BUILDFLAG(IS_WIN)
  // `extension` will be of std::wstring type on Windows which needs to be
  // handled differently than std::string. See base/files/file_path.h for more
  // info.
  result = base::WideToUTF8(extension);
#else
  result = extension;
#endif

  return result;
}

std::string GetFileName(std::string full_name, std::string file_extension) {
  return full_name.substr(0, full_name.size() - file_extension.size() - 1);
}

// These are simplified mappings derived from
// https://github.com/microsoft/fluentui/blob/master/packages/react-file-type-icons/src/FileTypeIconMap.ts.
GURL GetFileIconUrl(std::string mime_type) {
  const auto kIconMap =
      base::MakeFixedFlatMap<std::string_view, std::string_view>({
          // Audio files. Copied from `kStandardAudioTypes` in
          // net/base/mime_util.cc.
          {"audio/aac", kAudioIconPartialPath},
          {"audio/aiff", kAudioIconPartialPath},
          {"audio/amr", kAudioIconPartialPath},
          {"audio/basic", kAudioIconPartialPath},
          {"audio/flac", kAudioIconPartialPath},
          {"audio/midi", kAudioIconPartialPath},
          {"audio/mp3", kAudioIconPartialPath},
          {"audio/mp4", kAudioIconPartialPath},
          {"audio/mpeg", kAudioIconPartialPath},
          {"audio/mpeg3", kAudioIconPartialPath},
          {"audio/ogg", kAudioIconPartialPath},
          {"audio/vorbis", kAudioIconPartialPath},
          {"audio/wav", kAudioIconPartialPath},
          {"audio/webm", kAudioIconPartialPath},
          {"audio/x-m4a", kAudioIconPartialPath},
          {"audio/x-ms-wma", kAudioIconPartialPath},
          {"audio/vnd.rn-realaudio", kAudioIconPartialPath},
          {"audio/vnd.wave", kAudioIconPartialPath},
          // Image files. Copied from `kStandardImageTypes` in
          // net/base/mime_util.cc.
          {"image/avif", kImagesIconPartialPath},
          {"image/bmp", kImagesIconPartialPath},
          {"image/cis-cod", kImagesIconPartialPath},
          {"image/gif", kImagesIconPartialPath},
          {"image/heic", kImagesIconPartialPath},
          {"image/heif", kImagesIconPartialPath},
          {"image/ief", kImagesIconPartialPath},
          {"image/jpeg", kImagesIconPartialPath},
          {"image/pict", kImagesIconPartialPath},
          {"image/pipeg", kImagesIconPartialPath},
          {"image/png", kImagesIconPartialPath},
          {"image/webp", kImagesIconPartialPath},
          {"image/tiff", kImagesIconPartialPath},
          {"image/vnd.microsoft.icon", kImagesIconPartialPath},
          {"image/x-cmu-raster", kImagesIconPartialPath},
          {"image/x-cmx", kImagesIconPartialPath},
          {"image/x-icon", kImagesIconPartialPath},
          {"image/x-portable-anymap", kImagesIconPartialPath},
          {"image/x-portable-bitmap", kImagesIconPartialPath},
          {"image/x-portable-graymap", kImagesIconPartialPath},
          {"image/x-portable-pixmap", kImagesIconPartialPath},
          {"image/x-rgb", kImagesIconPartialPath},
          {"image/x-xbitmap", kImagesIconPartialPath},
          {"image/x-xpixmap", kImagesIconPartialPath},
          {"image/x-xwindowdump", kImagesIconPartialPath},
          // Video files. Copied from `kStandardVideoTypes` in
          // net/base/mime_util.cc.
          {"video/avi", kVideoIconPartialPath},
          {"video/divx", kVideoIconPartialPath},
          {"video/flc", kVideoIconPartialPath},
          {"video/mp4", kVideoIconPartialPath},
          {"video/mpeg", kVideoIconPartialPath},
          {"video/ogg", kVideoIconPartialPath},
          {"video/quicktime", kVideoIconPartialPath},
          {"video/sd-video", kVideoIconPartialPath},
          {"video/webm", kVideoIconPartialPath},
          {"video/x-dv", kVideoIconPartialPath},
          {"video/x-m4v", kVideoIconPartialPath},
          {"video/x-mpeg", kVideoIconPartialPath},
          {"video/x-ms-asf", kVideoIconPartialPath},
          {"video/x-ms-wmv", kVideoIconPartialPath},
          // Older versions of Microsoft Office files.
          {"application/msword", kXmlDocumentIconPartialPath},
          {"application/vnd.ms-excel", kXmlSpreadsheetIconPartialPath},
          {"pplication/vnd.ms-powerpoint", kXmlPresentationIconPartialPath},
          // OpenXML files.
          {"application/"
           "vnd.openxmlformats-officedocument.presentationml.presentation",
           kXmlPresentationIconPartialPath},
          {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
           kXmlSpreadsheetIconPartialPath},
          {"application/"
           "vnd.openxmlformats-officedocument.wordprocessingml.document",
           kXmlDocumentIconPartialPath},
          // Other file types.
          {"text/plain", kPlainTextIconPartialPath},
          {"application/csv", kCsvIconPartialPath},
          {"text/csv", kCsvIconPartialPath},
          {"application/pdf", kPdfIconPartialPath},
          {"application/rtf", kRichTextPartialPath},
          {"application/epub+zip", kRichTextPartialPath},
          {"application/zip", kZipPartialPath},
          {"text/xml", kXmlPartialPath},
          {"text/css", kCodeIconPartialPath},
          {"text/javascript", kCodeIconPartialPath},
          {"application/json", kCodeIconPartialPath},
          {"application/rdf+xml", kCodeIconPartialPath},
          {"application/rss+xml", kCodeIconPartialPath},
          {"text/x-sh", kCodeIconPartialPath},
          {"application/xhtml+xml", kCodeIconPartialPath},
          {"application/postscript", kVectorIconPartialPath},
          {"image/svg+xml", kVectorIconPartialPath},
      });

  const auto it = kIconMap.find(mime_type);
  if (it != kIconMap.end()) {
    return GURL(kBaseIconUrl).Resolve(std::string(it->second) + ".png");
  }
  return GURL();
}
}  // namespace microsoft_modules_helper
