// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#import <AppKit/AppKit.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_mac.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  return file_path.Extension();
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // NSWorkspace is thread-safe.
  return base::ThreadPool::CreateTaskRunner(traits());
}

void IconLoader::ReadIcon() {
  NSString* group = base::SysUTF8ToNSString(group_);
  NSWorkspace* workspace = [NSWorkspace sharedWorkspace];
  NSImage* icon = [workspace iconForFileType:group];

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
    gfx::ImageSkia image_skia(gfx::ImageSkiaFromResizedNSImage(icon, size));
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
