// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include <windows.h>

#include <shellapi.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread.h"
#include "chrome/browser/win/icon_reader_service.h"
#include "chrome/services/util_win/public/mojom/util_read_icon.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/icon_util.h"
#include "ui/gfx/image/image_skia.h"

namespace {
// Helper class to manage lifetime of icon reader service.
class IconLoaderHelper {
 public:
  static void ExecuteLoadIcon(
      base::FilePath filename,
      base::File file,
      chrome::mojom::IconSize size,
      float scale,
      gfx::Image default_icon,
      scoped_refptr<base::SingleThreadTaskRunner> target_task_runner,
      IconLoader::IconLoadedCallback icon_loaded_callback);

  IconLoaderHelper(base::FilePath filename,
                   chrome::mojom::IconSize size,
                   float scale,
                   gfx::Image default_icon);

  IconLoaderHelper(const IconLoaderHelper&) = delete;
  IconLoaderHelper& operator=(const IconLoaderHelper&) = delete;

 private:
  void StartReadIconRequest(base::File file);
  void OnConnectionError();
  void OnReadIconExecuted(const gfx::ImageSkia& icon);

  using IconLoaderHelperCallback =
      base::OnceCallback<void(gfx::Image image,
                              const IconLoader::IconGroup& icon_group)>;

  void set_finally(IconLoaderHelperCallback finally) {
    finally_ = std::move(finally);
  }

  mojo::Remote<chrome::mojom::UtilReadIcon> remote_read_icon_;
  base::FilePath filename_;
  chrome::mojom::IconSize size_;
  const float scale_;
  // This callback owns the object until work is done.
  IconLoaderHelperCallback finally_;
  gfx::Image default_icon_;

  SEQUENCE_CHECKER(sequence_checker_);
};

void IconLoaderHelper::ExecuteLoadIcon(
    base::FilePath filename,
    base::File file,
    chrome::mojom::IconSize size,
    float scale,
    gfx::Image default_icon,
    scoped_refptr<base::SingleThreadTaskRunner> target_task_runner,
    IconLoader::IconLoadedCallback icon_loaded_callback) {
  // Self-deleting helper manages service lifetime.
  auto helper = std::make_unique<IconLoaderHelper>(filename, size, scale,
                                                   std::move(default_icon));
  auto* helper_raw = helper.get();
  // This callback owns the helper and extinguishes itself once work is done.
  auto finally_callback = base::BindOnce(
      [](std::unique_ptr<IconLoaderHelper> helper,
         IconLoader::IconLoadedCallback icon_loaded_callback,
         scoped_refptr<base::SingleThreadTaskRunner> target_task_runner,
         gfx::Image image, const IconLoader::IconGroup& icon_group) {
        target_task_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(icon_loaded_callback),
                                      std::move(image), icon_group));
      },
      std::move(helper), std::move(icon_loaded_callback), target_task_runner);

  helper_raw->set_finally(std::move(finally_callback));
  helper_raw->StartReadIconRequest(std::move(file));
}

IconLoaderHelper::IconLoaderHelper(base::FilePath filename,
                                   chrome::mojom::IconSize size,
                                   float scale,
                                   gfx::Image default_icon)
    : filename_(filename),
      size_(size),
      scale_(scale),
      default_icon_(std::move(default_icon)) {
  remote_read_icon_ = LaunchIconReaderInstance();
  remote_read_icon_.set_disconnect_handler(base::BindOnce(
      &IconLoaderHelper::OnConnectionError, base::Unretained(this)));
}

void IconLoaderHelper::StartReadIconRequest(base::File file) {
  remote_read_icon_->ReadIcon(
      std::move(file), size_, scale_,
      base::BindOnce(&IconLoaderHelper::OnReadIconExecuted,
                     base::Unretained(this)));
}

void IconLoaderHelper::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (finally_.is_null())
    return;

  std::move(finally_).Run(std::move(default_icon_), filename_.value());
}

void IconLoaderHelper::OnReadIconExecuted(const gfx::ImageSkia& icon) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::wstring icon_group = filename_.value();
  if (icon.isNull()) {
    std::move(finally_).Run(std::move(default_icon_), icon_group);
  } else {
    gfx::Image image(icon);
    std::move(finally_).Run(std::move(image), icon_group);
  }
}

// Must be called in a COM context. |group| should be a file extension.
gfx::Image GetIconForFileExtension(const std::wstring& group,
                                   IconLoader::IconSize icon_size) {
  int size = 0;
  switch (icon_size) {
    case IconLoader::SMALL:
      size = SHGFI_SMALLICON;
      break;
    case IconLoader::NORMAL:
      size = 0;
      break;
    case IconLoader::LARGE:
      size = SHGFI_LARGEICON;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  gfx::Image image;

  // Not only is GetFileInfo a blocking call, it's also known to hang
  // (crbug.com/1249943), add a ScopedBlockingCall to let the scheduler know
  // when this hangs and to explicitly label this call in tracing.
  base::ScopedBlockingCall blocking_call(FROM_HERE,
                                         base::BlockingType::MAY_BLOCK);

  SHFILEINFO file_info = {0};
  if (SHGetFileInfo(group.c_str(), FILE_ATTRIBUTE_NORMAL, &file_info,
                    sizeof(file_info),
                    SHGFI_ICON | size | SHGFI_USEFILEATTRIBUTES)) {
    const SkBitmap bitmap = IconUtil::CreateSkBitmapFromHICON(file_info.hIcon);
    if (!bitmap.isNull()) {
      gfx::ImageSkia image_skia(
          gfx::ImageSkiaRep(bitmap, display::win::GetDPIScale()));
      image_skia.MakeThreadSafe();
      image = gfx::Image(image_skia);
    }
    DestroyIcon(file_info.hIcon);
  }
  return image;
}

}  // namespace

// static
IconLoader::IconGroup IconLoader::GroupForFilepath(
    const base::FilePath& file_path) {
  if (file_path.MatchesExtension(L".exe") ||
      file_path.MatchesExtension(L".dll") ||
      file_path.MatchesExtension(L".ico")) {
    return file_path.value();
  }

  return file_path.Extension();
}

// static
scoped_refptr<base::TaskRunner> IconLoader::GetReadIconTaskRunner() {
  // Technically speaking, only a thread with COM is needed, not one that has
  // a COM STA. However, this is what is available for now.
  return base::ThreadPool::CreateCOMSTATaskRunner(traits());
}

void IconLoader::ReadGroup() {
  group_ = GroupForFilepath(file_path_);

  if (group_ == file_path_.value()) {
    // Calls a Windows API that parses the file so must be sandboxed.
    GetReadIconTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IconLoader::ReadIconInSandbox, base::Unretained(this)));
  } else {
    // Looks up generic icons for groups based only on the file's extension.
    GetReadIconTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&IconLoader::ReadIcon, base::Unretained(this)));
  }
}

void IconLoader::ReadIcon() {
  auto image = GetIconForFileExtension(group_, icon_size_);

  target_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(image), group_));

  delete this;
}

void IconLoader::ReadIconInSandbox() {
  // Get default first as loader is deleted before ExecuteLoadIcon
  // completes.
  auto path = base::FilePath(group_);
  auto default_icon = GetIconForFileExtension(path.Extension(), icon_size_);

  chrome::mojom::IconSize size = chrome::mojom::IconSize::kNormal;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size = chrome::mojom::IconSize::kSmall;
      break;
    case IconLoader::NORMAL:
      size = chrome::mojom::IconSize::kNormal;
      break;
    case IconLoader::LARGE:
      size = chrome::mojom::IconSize::kLarge;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  base::File file;
  file.Initialize(path, base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE |
                            base::File::FLAG_OPEN);
  if (file.IsValid()) {
    target_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IconLoaderHelper::ExecuteLoadIcon, std::move(path),
                       std::move(file), size, scale_, std::move(default_icon),
                       target_task_runner_, std::move(callback_)));
  } else {
    target_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(default_icon),
                                  path.value()));
  }
  delete this;
}
