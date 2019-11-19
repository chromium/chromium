// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "media/media_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

// Used with GenerateImageWithSize() to indicate that the image shouldn't be
// resized.
const int kDoNotResize = -1;

struct IdrBySize {
  int idr_small;
  int idr_normal;
  int idr_large;
};

// Performs mapping of <file extension, icon size> to icon resource IDs.
class IconMapper {
 public:
  IconMapper();

  // Lookup icon resource ID for a given filename |extension| and
  // |icon_size|. Defaults to generic icons if there are no icons for the given
  // extension.
  int Lookup(const std::string& extension, IconLoader::IconSize icon_size);

 private:
  typedef std::map<std::string, IdrBySize> ExtensionIconMap;

  ExtensionIconMap extension_icon_map_;
};

const IdrBySize kAudioIdrs = {
  IDR_FILETYPE_AUDIO,
  IDR_FILETYPE_LARGE_AUDIO,
  IDR_FILETYPE_LARGE_AUDIO
};
const IdrBySize kGenericIdrs = {
  IDR_FILETYPE_GENERIC,
  IDR_FILETYPE_LARGE_GENERIC,
  IDR_FILETYPE_LARGE_GENERIC
};
const IdrBySize kImageIdrs = {
  IDR_FILETYPE_IMAGE,
  IDR_FILETYPE_IMAGE,
  IDR_FILETYPE_IMAGE
};
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const IdrBySize kPdfIdrs = {
  IDR_FILETYPE_PDF,
  IDR_FILETYPE_PDF,
  IDR_FILETYPE_PDF
};
#endif
const IdrBySize kVideoIdrs = {
  IDR_FILETYPE_VIDEO,
  IDR_FILETYPE_LARGE_VIDEO,
  IDR_FILETYPE_LARGE_VIDEO
};

IconMapper::IconMapper() {
  // The code below should match translation in
  // ui/file_manager/file_manager/js/file_manager.js
  // ui/file_manager/file_manager/css/file_manager.css
  // 'audio': /\.(mp3|m4a|oga|ogg|wav)$/i,
  // 'html': /\.(html?)$/i,
  // 'image': /\.(bmp|gif|jpe?g|ico|png|webp)$/i,
  // 'pdf' : /\.(pdf)$/i,
  // 'text': /\.(pod|rst|txt|log)$/i,
  // 'video': /\.(mov|mp4|m4v|mpe?g4?|ogm|ogv|ogx|webm)$/i

  const ExtensionIconMap::value_type kExtensionIdrBySizeData[] = {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    std::make_pair(".m4a", kAudioIdrs),
    std::make_pair(".mp3", kAudioIdrs),
    std::make_pair(".pdf", kPdfIdrs),
    std::make_pair(".3gp", kVideoIdrs),
    std::make_pair(".avi", kVideoIdrs),
    std::make_pair(".m4v", kVideoIdrs),
    std::make_pair(".mov", kVideoIdrs),
    std::make_pair(".mp4", kVideoIdrs),
    std::make_pair(".mpeg", kVideoIdrs),
    std::make_pair(".mpg", kVideoIdrs),
    std::make_pair(".mpeg4", kVideoIdrs),
    std::make_pair(".mpg4", kVideoIdrs),
#endif
    std::make_pair(".flac", kAudioIdrs),
    std::make_pair(".oga", kAudioIdrs),
    std::make_pair(".ogg", kAudioIdrs),
    std::make_pair(".wav", kAudioIdrs),
    std::make_pair(".bmp", kImageIdrs),
    std::make_pair(".gif", kImageIdrs),
    std::make_pair(".ico", kImageIdrs),
    std::make_pair(".jpeg", kImageIdrs),
    std::make_pair(".jpg", kImageIdrs),
    std::make_pair(".png", kImageIdrs),
    std::make_pair(".webp", kImageIdrs),
    std::make_pair(".ogm", kVideoIdrs),
    std::make_pair(".ogv", kVideoIdrs),
    std::make_pair(".ogx", kVideoIdrs),
    std::make_pair(".webm", kVideoIdrs),
  };

  const size_t kESize = base::size(kExtensionIdrBySizeData);
  ExtensionIconMap source(&kExtensionIdrBySizeData[0],
                          &kExtensionIdrBySizeData[kESize]);
  extension_icon_map_.swap(source);
}

int IconMapper::Lookup(const std::string& extension,
                       IconLoader::IconSize icon_size) {
  DCHECK(icon_size == IconLoader::SMALL ||
         icon_size == IconLoader::NORMAL ||
         icon_size == IconLoader::LARGE);
  ExtensionIconMap::const_iterator it = extension_icon_map_.find(extension);
  const IdrBySize& idrbysize =
      ((it == extension_icon_map_.end()) ? kGenericIdrs : it->second);
  int idr = -1;
  switch (icon_size) {
    case IconLoader::SMALL:  idr = idrbysize.idr_small;  break;
    case IconLoader::NORMAL: idr = idrbysize.idr_normal; break;
    case IconLoader::LARGE:  idr = idrbysize.idr_large;  break;
    case IconLoader::ALL:
    default:
      NOTREACHED();
  }
  return idr;
}

// Returns a copy of |source| that is |dip_size| in width and height. If
// |dip_size| is |kDoNotResize|, returns an unmodified copy of |source|.
// |source| must be a square image (width == height).
gfx::ImageSkia ResizeImage(const gfx::ImageSkia& source, int dip_size) {
  DCHECK(!source.isNull());
  DCHECK(source.width() == source.height());

  if (dip_size == kDoNotResize || source.width() == dip_size)
    return source;

  return gfx::ImageSkiaOperations::CreateResizedImage(source,
      skia::ImageOperations::RESIZE_BEST, gfx::Size(dip_size, dip_size));
}

int IconSizeToDIPSize(IconLoader::IconSize size) {
  switch (size) {
    case IconLoader::SMALL:  return 16;
    case IconLoader::NORMAL: return 32;
    case IconLoader::LARGE:  // fallthrough
      // On ChromeOS, we consider LARGE to mean "the largest image we have."
      // Since we have already chosen the largest applicable image resource, we
      // return the image as-is.
    case IconLoader::ALL:    // fallthrough
    default:
      return kDoNotResize;
  }
}

}  // namespace

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  return base::ToLowerASCII(file_path.Extension());
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // ReadIcon touches non thread safe ResourceBundle images, so it must be on
  // the UI thread.
  return base::CreateSingleThreadTaskRunner({content::BrowserThread::UI});
}

void IconLoader::ReadIcon() {
  static base::LazyInstance<IconMapper>::Leaky icon_mapper =
      LAZY_INSTANCE_INITIALIZER;
  int idr = icon_mapper.Get().Lookup(group_, icon_size_);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia image_skia(ResizeImage(*(rb.GetImageNamed(idr)).ToImageSkia(),
                                        IconSizeToDIPSize(icon_size_)));
  image_skia.MakeThreadSafe();
  gfx::Image image(image_skia);
  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));
  delete this;
}
