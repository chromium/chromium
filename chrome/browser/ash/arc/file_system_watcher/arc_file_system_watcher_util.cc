// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/strings/string_util.h"

namespace arc {

// The set of media file extensions supported by Android MediaScanner.
// Entries must be lower-case and sorted by lexicographical order for
// binary search.
//
// The current list includes all the extensions supported in P, R or T.
// For P, the set of supported extension is obtained from
// frameworks/base/media/java/android/media/MediaFile.java.
// For R+, it is calculated from the set of MIME types that map to "Image",
// "Audio", "Video", "Document", "Playlist", or "Subtitle" media type as defined
// in .../MediaProvider/src/com/android/providers/media/util/MimeUtils.java,
// and the extension-to-MIME-type mappings defined in
// external/mime-support/mime.types and
// frameworks/base/mime/java-res/android.mime.types.
//
// The comment for each extension shows its corresponding MIME type in T.
constexpr auto kAndroidSupportedMediaExtensions = base::MakeFixedFlatSet<
    std::string_view>({
    ".323",       // text/h323
    ".3g2",       // video/3gpp2
    ".3ga",       // audio/3gpp
    ".3gp",       // video/3gpp
    ".3gp2",      // video/3gpp2
    ".3gpp",      // video/3gpp
    ".3gpp2",     // video/3gpp2
    ".a52",       // audio/ac3
    ".aac",       // audio/aac
    ".ac3",       // audio/ac3
    ".adt",       // audio/aac
    ".adts",      // audio/aac
    ".aif",       // audio/x-aiff
    ".aifc",      // audio/x-aiff
    ".aiff",      // audio/x-aiff
    ".amr",       // audio/amr
    ".appcache",  // text/cache-manifest
    ".art",       // image/x-jg
    ".arw",       // image/x-sony-arw
    ".asc",       // text/plain
    ".asf",       // video/x-ms-asf
    ".asx",       // video/x-ms-asf
    ".au",        // audio/basic
    ".avi",       // video/avi
    ".avif",      // image/avif
    ".awb",       // audio/amr-wb
    ".axa",       // audio/annodex
    ".axv",       // video/annodex
    ".bib",       // text/x-bibtex
    ".bmp",       // image/x-ms-bmp
    ".boo",       // text/x-boo
    ".brf",       // text/plain
    ".c",         // text/x-csrc
    ".c++",       // text/x-c++src
    ".cc",        // text/x-c++src
    ".cdr",       // image/x-coreldraw
    ".cdt",       // image/x-coreldrawtemplate
    ".cls",       // text/x-tex
    ".cpp",       // text/x-c++src
    ".cpt",       // image/x-corelphotopaint
    ".cr2",       // image/x-canon-cr2
    ".crw",       // image/x-canon-crw
    ".csd",       // audio/csound
    ".csh",       // text/x-csh
    ".css",       // text/css
    ".csv",       // text/comma-separated-values
    ".cur",       // image/ico
    ".cxx",       // text/x-c++src
    ".d",         // text/x-dsrc
    ".dfxp",      // application/ttml+xml
    ".dif",       // video/dv
    ".diff",      // text/plain
    ".djv",       // image/vnd.djvu
    ".djvu",      // image/vnd.djvu
    ".dl",        // video/dl
    ".dng",       // image/x-adobe-dng
    ".doc",       // application/msword
    // application/vnd.openxmlformats-officedocument.wordprocessingml.document
    ".docx",
    ".dot",  // application/msword
    // application/vnd.openxmlformats-officedocument.wordprocessingml.template
    ".dotx",
    ".dv",        // video/dv
    ".epub",      // application/epub+zip
    ".erf",       // image/x-epson-erf
    ".etx",       // text/x-setext
    ".f4a",       // audio/mp4
    ".f4b",       // audio/mp4
    ".f4p",       // audio/mp4
    ".f4v",       // video/mp4
    ".fl",        // application/x-android-drm-fl (supported as DRM file)
    ".flac",      // audio/flac
    ".fli",       // video/fli
    ".flv",       // video/x-flv
    ".gcd",       // text/x-pcs-gcd
    ".gif",       // image/gif
    ".gl",        // video/gl
    ".gsm",       // audio/x-gsm
    ".h",         // text/x-chdr
    ".h++",       // text/x-c++hdr
    ".heic",      // image/heic
    ".heics",     // image/heic-sequence
    ".heif",      // image/heif
    ".heifs",     // image/heif-sequence
    ".hh",        // text/x-c++hdr
    ".hif",       // image/heif
    ".hpp",       // text/x-c++hdr
    ".hs",        // text/x-haskell
    ".htc",       // text/x-component
    ".htm",       // text/html
    ".html",      // text/html
    ".hxx",       // text/x-c++hdr
    ".ico",       // image/x-icon
    ".ics",       // text/calendar
    ".icz",       // text/calendar
    ".ief",       // image/ief
    ".imy",       // audio/imelody
    ".jad",       // text/vnd.sun.j2me.app-descriptor
    ".java",      // text/x-java
    ".jng",       // image/x-jng
    ".jp2",       // image/jp2
    ".jpe",       // image/jpeg
    ".jpeg",      // image/jpeg
    ".jpf",       // image/jpx
    ".jpg",       // image/jpeg
    ".jpg2",      // image/jp2
    ".jpm",       // image/jpm
    ".jpx",       // image/jpx
    ".kar",       // audio/midi
    ".lhs",       // text/x-literate-haskell
    ".lrc",       // application/lrc
    ".lsf",       // video/x-la-asf
    ".lsx",       // video/x-la-asf
    ".ltx",       // text/x-tex
    ".ly",        // text/x-lilypond
    ".m1v",       // video/mpeg
    ".m2t",       // video/mpeg
    ".m2ts",      // video/mp2t
    ".m2v",       // video/mpeg
    ".m3u",       // audio/x-mpegurl
    ".m3u8",      // audio/x-mpegurl
    ".m4a",       // audio/mpeg
    ".m4b",       // audio/mp4
    ".m4p",       // audio/mp4
    ".m4r",       // audio/mpeg
    ".m4v",       // video/mp4
    ".markdown",  // text/markdown
    ".md",        // text/markdown
    ".mid",       // audio/midi
    ".midi",      // audio/midi
    ".mka",       // audio/x-matroska
    ".mkv",       // video/x-matroska
    ".mml",       // text/mathml
    ".mng",       // video/x-mng
    ".moc",       // text/x-moc
    ".mov",       // video/quicktime
    ".movie",     // video/x-sgi-movie
    ".mp1",       // audio/mpeg
    ".mp1v",      // video/mpeg
    ".mp2",       // audio/mpeg
    ".mp2v",      // video/mpeg
    ".mp3",       // audio/mpeg
    ".mp4",       // video/mp4
    ".mp4v",      // video/mp4
    ".mpa",       // audio/mpeg
    ".mpe",       // video/mpeg
    ".mpeg",      // video/mpeg
    ".mpeg1",     // video/mpeg
    ".mpeg2",     // video/mpeg
    ".mpeg4",     // video/mp4
    ".mpega",     // audio/mpeg
    ".mpg",       // video/mpeg
    ".mpga",      // audio/mpeg
    ".mpv",       // video/x-matroska
    ".mpv1",      // video/mpeg
    ".mpv2",      // video/mpeg
    ".mts",       // video/mp2t
    ".mxmf",      // audio/mobile-xmf
    ".mxu",       // video/vnd.mpegurl
    ".nef",       // image/x-nikon-nef
    ".nrw",       // image/x-nikon-nrw
    ".odb",       // application/vnd.oasis.opendocument.database
    ".odc",       // application/vnd.oasis.opendocument.chart
    ".odf",       // application/vnd.oasis.opendocument.formula
    ".odg",       // application/vnd.oasis.opendocument.graphics
    ".odm",       // application/vnd.oasis.opendocument.text-master
    ".odp",       // application/vnd.oasis.opendocument.presentation
    ".ods",       // application/vnd.oasis.opendocument.spreadsheet
    ".odt",       // application/vnd.oasis.opendocument.text
    ".oga",       // audio/ogg
    ".ogg",       // audio/ogg
    ".ogv",       // video/ogg
    ".opus",      // audio/ogg
    ".orc",       // audio/csound
    ".orf",       // image/x-olympus-orf
    ".ota",       // application/vnd.android.ota (supported only in P as
                  // audio/midi)
    ".otg",       // application/vnd.oasis.opendocument.graphics-template
    ".oth",       // application/vnd.oasis.opendocument.text-web
    ".otp",       // application/vnd.oasis.opendocument.presentation-template
    ".ots",       // application/vnd.oasis.opendocument.spreadsheet-template
    ".ott",       // application/vnd.oasis.opendocument.text-template
    ".p",         // text/x-pascal
    ".pas",       // text/x-pascal
    ".pat",       // image/x-coreldrawpattern
    ".patch",     // text/x-diff
    ".pbm",       // image/x-portable-bitmap
    ".pcx",       // image/pcx
    ".pdf",       // application/pdf
    ".pef",       // image/x-pentax-pef
    ".pgm",       // image/x-portable-graymap
    ".phps",      // text/text
    ".pl",        // text/x-perl
    ".pls",       // audio/x-scpls
    ".pm",        // text/x-perl
    ".png",       // image/png
    ".pnm",       // image/x-portable-anymap
    ".po",        // text/plain
    ".pot",       // application/vnd.ms-powerpoint
    // application/vnd.openxmlformats-officedocument.presentationml.template
    ".potx",
    ".ppm",  // image/x-portable-pixmap
    ".pps",  // application/vnd.ms-powerpoint
    // application/vnd.openxmlformats-officedocument.presentationml.slideshow
    ".ppsx",
    ".ppt",  // application/vnd.ms-powerpoint
    // application/vnd.openxmlformats-officedocument.presentationml.presentation
    ".pptx",
    ".psd",    // image/x-photoshop
    ".py",     // text/x-python
    ".qt",     // video/quicktime
    ".ra",     // audio/x-pn-realaudio
    ".raf",    // image/x-fuji-raf
    ".ram",    // audio/x-pn-realaudio
    ".ras",    // image/x-cmu-raster
    ".rgb",    // image/x-rgb
    ".rm",     // audio/x-pn-realaudio
    ".rtf",    // text/rtf
    ".rtttl",  // audio/midi
    ".rtx",    // audio/midi
    ".rw2",    // image/x-panasonic-rw2
    ".scala",  // text/x-scala
    ".sco",    // audio/csound
    ".sct",    // text/scriptlet
    ".sd2",    // audio/x-sd2
    ".sda",    // application/vnd.stardivision.draw
    ".sdc",    // application/vnd.stardivision.calc
    ".sdd",    // application/vnd.stardivision.impress
    ".sdp",    // application/vnd.stardivision.impress
    ".sds",    // application/vnd.stardivision.chart
    ".sdw",    // application/vnd.stardivision.writer
    ".sfv",    // text/x-sfv
    ".sgl",    // application/vnd.stardivision.writer-global
    ".sh",     // text/x-sh
    ".shtml",  // text/html
    ".sid",    // audio/prs.sid
    ".smf",    // audio/sp-midi
    ".smi",    // application/smil+xml
    ".smil",   // application/smil+xml
    ".snd",    // audio/basic
    ".spx",    // audio/ogg
    ".srt",    // application/x-subrip
    ".srw",    // image/x-samsung-srw
    ".stc",    // application/vnd.sun.xml.calc.template
    ".std",    // application/vnd.sun.xml.draw.template
    ".sti",    // application/vnd.sun.xml.impress.template
    ".sty",    // text/x-tex
    ".svg",    // image/svg+xml
    ".svgz",   // image/svg+xml
    ".sxc",    // application/vnd.sun.xml.calc
    ".sxd",    // application/vnd.sun.xml.draw
    ".sxg",    // application/vnd.sun.xml.writer.global
    ".sxi",    // application/vnd.sun.xml.impress
    ".sxm",    // application/vnd.sun.xml.math
    ".sxw",    // application/vnd.sun.xml.writer
    ".tcl",    // text/x-tcl
    ".tex",    // text/x-tex
    ".text",   // text/plain
    ".tif",    // image/tiff
    ".tiff",   // image/tiff
    ".tk",     // text/x-tcl
    ".tm",     // text/texmacs
    ".ts",     // video/mp2ts
    ".tsv",    // text/tab-separated-values
    ".ttl",    // text/turtle
    ".ttml",   // application/ttml+xml
    ".txt",    // text/plain
    ".uls",    // text/iuls
    ".vcard",  // text/vcard
    ".vcf",    // text/x-vcard
    ".vcs",    // text/x-vcalendar
    ".vor",    // application/vnd.stardivision.writer
    ".wav",    // audio/x-wav
    ".wax",    // audio/x-ms-wax
    ".wbmp",   // image/vnd.wap.wbmp
    ".webm",   // video/webm
    ".webp",   // image/webp
    ".wm",     // video/x-ms-wm
    ".wma",    // audio/x-ms-wma
    ".wml",    // text/vnd.wap.wml
    ".wmls",   // text/vnd.wap.wmlscript
    ".wmv",    // video/x-ms-wmv
    ".wmx",    // video/x-ms-wmx
    ".wpl",    // application/vnd.ms-wpl
    ".wrf",    // video/x-webex
    ".wsc",    // text/scriptlet
    ".wvx",    // video/x-ms-wvx
    ".xbm",    // image/x-xbitmap
    ".xlb",    // application/vnd.ms-excel
    ".xls",    // application/vnd.ms-excel
    // application/vnd.openxmlformats-officedocument.spreadsheetml.sheet
    ".xlsx",
    ".xlt",  // application/vnd.ms-excel
    // application/vnd.openxmlformats-officedocument.spreadsheetml.template
    ".xltx",
    ".xmf",   // audio/midi
    ".xml",   // text/xml
    ".xpm",   // image/x-xpixmap
    ".xspf",  // application/xspf+xml
    ".xwd",   // image/x-xwindowdump
    ".yt",    // video/vnd.youtube.yt
});

bool AppendRelativePathForRemovableMedia(const base::FilePath& cros_path,
                                         base::FilePath* android_path) {
  std::vector<base::FilePath::StringType> parent_components =
      base::FilePath(kCrosRemovableMediaDir).GetComponents();
  std::vector<base::FilePath::StringType> child_components =
      cros_path.GetComponents();
  auto child_itr = child_components.begin();
  for (const auto& parent_component : parent_components) {
    if (child_itr == child_components.end() || parent_component != *child_itr) {
      LOG(WARNING) << "|cros_path| is not under kCrosRemovableMediaDir.";
      return false;
    }
    ++child_itr;
  }
  if (child_itr == child_components.end()) {
    LOG(WARNING) << "The CrOS path doesn't have a component for device label.";
    return false;
  }
  // The device label (e.g. "UNTITLED" for /media/removable/UNTITLED/foo.jpg)
  // should be converted to removable_UNTITLED, since the prefix "removable_"
  // is appended to paths for removable media in Android.
  *android_path = android_path->Append(kRemovableMediaLabelPrefix + *child_itr);
  ++child_itr;
  for (; child_itr != child_components.end(); ++child_itr) {
    *android_path = android_path->Append(*child_itr);
  }
  return true;
}

base::FilePath GetAndroidPath(const base::FilePath& cros_path,
                              const base::FilePath& cros_dir,
                              const base::FilePath& android_dir) {
  base::FilePath android_path(android_dir);
  if (cros_dir.value() == kCrosRemovableMediaDir) {
    if (!AppendRelativePathForRemovableMedia(cros_path, &android_path))
      return base::FilePath();
  } else {
    cros_dir.AppendRelativePath(cros_path, &android_path);
  }
  return android_path;
}

bool HasAndroidSupportedMediaExtension(const base::FilePath& path) {
  return kAndroidSupportedMediaExtensions.contains(
      base::ToLowerASCII(path.Extension()));
}

}  // namespace arc
