// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>                      // pre-macOS 11
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>  // macOS 11

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  // The best option is to get the type directly from the file. The next best
  // option is to pull the extension from the file and get the type from that.
  // The last and worst option is to fall back to `public.content` which will
  // give a generic file icon.

  if (@available(macOS 11, *)) {
    UTType* type;
    NSURL* file_url = base::apple::FilePathToNSURL(file_path);
    if (file_url && [file_url getResourceValue:&type
                                        forKey:NSURLContentTypeKey
                                         error:nil]) {
      return base::SysNSStringToUTF8(type.identifier);
    }

    std::string extension_string = file_path.FinalExtension();
    if (!extension_string.empty()) {
      // Remove the leading dot.
      extension_string.erase(extension_string.begin());

      type = [UTType
          typeWithFilenameExtension:base::SysUTF8ToNSString(extension_string)];
      if (type) {
        return base::SysNSStringToUTF8(type.identifier);
      }
    }

    return base::SysNSStringToUTF8(UTTypeContent.identifier);
  } else {
    NSString* type;
    NSURL* file_url = base::apple::FilePathToNSURL(file_path);
    if (file_url && [file_url getResourceValue:&type
                                        forKey:NSURLTypeIdentifierKey
                                         error:nil]) {
      return base::SysNSStringToUTF8(type);
    }

    std::string extension_string = file_path.FinalExtension();
    if (!extension_string.empty()) {
      // Remove the leading dot.
      extension_string.erase(extension_string.begin());

      base::apple::ScopedCFTypeRef<CFStringRef> extension_cf =
          base::SysUTF8ToCFStringRef(extension_string);
      base::apple::ScopedCFTypeRef<CFStringRef> cftype(
          UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                                extension_cf,
                                                /*inConformingToUTI=*/nullptr));
      if (cftype) {
        return base::SysCFStringRefToUTF8(cftype);
      }
    }

    return base::SysCFStringRefToUTF8(kUTTypeContent);
  }
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // NSWorkspace is thread-safe.
  return base::ThreadPool::CreateTaskRunner(traits());
}

void IconLoader::ReadIcon() {
  NSImage* icon;
  if (@available(macOS 11, *)) {
    UTType* type = [UTType typeWithIdentifier:base::SysUTF8ToNSString(group_)];
    icon = [NSWorkspace.sharedWorkspace iconForContentType:type];
  } else {
    NSString* type = base::SysUTF8ToNSString(group_);
    icon = [NSWorkspace.sharedWorkspace iconForFileType:type];
  }

  gfx::Image image;
  if (icon_size_ == ALL) {
    // The NSImage already has all sizes.
    image = gfx::Image(icon);
  } else {
    NSSize size = NSZeroSize;
    switch (icon_size_) {
      case IconLoader::SMALL:
        size = NSMakeSize(16, 16);
        break;
      case IconLoader::NORMAL:
        size = NSMakeSize(32, 32);
        break;
      default:
        NOTREACHED();
    }

    gfx::ImageSkia image_skia = gfx::ImageSkiaFromResizedNSImage(icon, size);
    if (!image_skia.isNull()) {
      image_skia.MakeThreadSafe();
      image = gfx::Image(image_skia);
    }
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));
  delete this;
}
