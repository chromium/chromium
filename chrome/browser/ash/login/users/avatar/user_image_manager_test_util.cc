// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/login/users/avatar/user_image_manager_test_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "ipc/constants.mojom.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash::test {

const char kUserAvatarImage1RelativePath[] = "chromeos/avatars/avatar1.jpg";
const char kUserAvatarImage2RelativePath[] = "chromeos/avatars/avatar2.jpg";
const char kUserAvatarImage3RelativePath[] = "chromeos/avatars/avatar3.png";

bool AreImagesEqual(const gfx::ImageSkia& first, const gfx::ImageSkia& second) {
  if (first.width() != second.width() || first.height() != second.height()) {
    return false;
  }
  const SkBitmap* first_bitmap = first.bitmap();
  const SkBitmap* second_bitmap = second.bitmap();
  if (!first_bitmap && !second_bitmap) {
    return true;
  }
  if (!first_bitmap || !second_bitmap) {
    return false;
  }

  const size_t size = first_bitmap->computeByteSize();
  if (second_bitmap->computeByteSize() != size) {
    return false;
  }

  uint8_t* first_data = reinterpret_cast<uint8_t*>(first_bitmap->getPixels());
  uint8_t* second_data = reinterpret_cast<uint8_t*>(second_bitmap->getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i]) {
      return false;
    }
  }
  return true;
}

// *****************************************************************************
// ImageLoader
// *****************************************************************************

ImageLoader::ImageLoader(const base::FilePath& path) : path_(path) {}

ImageLoader::~ImageLoader() = default;

gfx::ImageSkia ImageLoader::Load() {
  std::string image_data;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    ReadFileToString(path_, &image_data);
  }

  const data_decoder::mojom::ImageCodec codec =
      (path_.Extension() == FILE_PATH_LITERAL(".jpg")
           ? data_decoder::mojom::ImageCodec::kDefault
           : data_decoder::mojom::ImageCodec::kPng);

  base::test::TestFuture<const SkBitmap&> future;

  data_decoder::DecodeImageIsolated(
      base::as_byte_span(image_data), codec,
      /*shrink_to_fit=*/false,
      static_cast<int64_t>(IPC::mojom::kChannelMaximumMessageSize),
      /*desired_image_frame_size=*/gfx::Size(), future.GetCallback());

  // Waits until the callback is called.
  const SkBitmap& decoded_image = future.Get();
  if (decoded_image.isNull()) {
    return gfx::ImageSkia();
  } else {
    return gfx::ImageSkia::CreateFromBitmap(decoded_image, 1.0f);
  }
}

// *****************************************************************************
// UserImageChangeWaiter
// *****************************************************************************

UserImageChangeWaiter::UserImageChangeWaiter() = default;

UserImageChangeWaiter::~UserImageChangeWaiter() = default;

void UserImageChangeWaiter::Wait() {
  user_manager::UserManager::Get()->AddObserver(this);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void UserImageChangeWaiter::OnUserImageChanged(const user_manager::User& user) {
  if (run_loop_) {
    run_loop_->Quit();
  }
}

}  // namespace ash::test
