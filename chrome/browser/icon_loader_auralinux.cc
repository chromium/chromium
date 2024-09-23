// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/functional/bind.h"
#include "base/nix/mime_util_xdg.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/linux/linux_ui.h"

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  return base::nix::GetFileMimeType(file_path);
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // ReadIcon() calls into ui::LinuxUi and GTK code, so it must be on the UI
  // thread.
  return content::GetUIThreadTaskRunner({});
}

void IconLoader::ReadIcon() {
  int size_pixels = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size_pixels = 16;
      break;
    case IconLoader::NORMAL:
      size_pixels = 32;
      break;
    case IconLoader::LARGE:
      size_pixels = 48;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  gfx::Image image;
  ui::LinuxUi* ui = ui::LinuxUi::instance();
  if (ui) {
    image = ui->GetIconForContentType(group_, size_pixels, scale_);
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));
  delete this;
}
