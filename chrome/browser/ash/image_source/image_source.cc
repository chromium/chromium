// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/image_source/image_source.h"

#include <stddef.h>

#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/browser/ash/login/users/avatar/user_image_loader.h"
#include "chrome/common/url_constants.h"
#include "components/user_manager/user_image/user_image.h"
#include "net/base/mime_util.h"

namespace ash {
namespace {

const char* const kAllowlistedDirectories[] = {"regulatory_labels"};

// Callback for user_manager::UserImageLoader.
void ImageLoaded(content::URLDataSource::GotDataCallback got_data_callback,
                 std::unique_ptr<user_manager::UserImage> user_image) {
  if (user_image->has_image_bytes())
    std::move(got_data_callback).Run(user_image->image_bytes());
  else
    std::move(got_data_callback).Run(nullptr);
}

}  // namespace

ImageSource::ImageSource() {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

ImageSource::~ImageSource() {
}

std::string ImageSource::GetSource() {
  return chrome::kChromeOSAssetHost;
}

void ImageSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback got_data_callback) {
  const std::string path = content::URLDataSource::URLToRequestPath(url);
  if (!IsAllowlisted(path)) {
    std::move(got_data_callback).Run(nullptr);
    return;
  }

  const base::FilePath asset_dir(chrome::kChromeOSAssetPath);
  const base::FilePath image_path = asset_dir.AppendASCII(path);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::PathExists, image_path),
      base::BindOnce(&ImageSource::StartDataRequestAfterPathExists,
                     weak_factory_.GetWeakPtr(), image_path,
                     std::move(got_data_callback)));
}

void ImageSource::StartDataRequestAfterPathExists(
    const base::FilePath& image_path,
    content::URLDataSource::GotDataCallback got_data_callback,
    bool path_exists) {
  if (path_exists) {
    user_image_loader::StartWithFilePath(
        task_runner_, image_path, ImageDecoder::DEFAULT_CODEC,
        0,  // Do not crop.
        base::BindOnce(&ImageLoaded, std::move(got_data_callback)));
  } else {
    std::move(got_data_callback).Run(nullptr);
  }
}

std::string ImageSource::GetMimeType(const GURL& url) {
  std::string mime_type;
  std::string ext = base::FilePath(url.path_piece()).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}

bool ImageSource::IsAllowlisted(const std::string& path) const {
  base::FilePath file_path(path);
  if (file_path.ReferencesParent())
    return false;

  // Check if the path starts with a allowlisted directory.
  std::vector<std::string> components = file_path.GetComponents();
  if (components.empty())
    return false;

  for (size_t i = 0; i < std::size(kAllowlistedDirectories); i++) {
    if (components[0] == kAllowlistedDirectories[i])
      return true;
  }
  return false;
}

}  // namespace ash
