// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_image_store.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zstd/src/lib/zstd.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::quick_pair {

// Alias DeviceImageInfo for convenience.
using bluetooth_config::DeviceImageInfo;

namespace {

// zstd level 1 is the fastest standard level.
constexpr int kZstdCompressionLevel = 1;

// Returns |image| compressed with zstd, or an empty string if |image| is
// empty.
std::string CompressImage(std::string_view image) {
  if (image.empty()) {
    return std::string();
  }
  std::string compressed(ZSTD_compressBound(image.size()), '\0');
  const size_t compressed_size =
      ZSTD_compress(compressed.data(), compressed.size(), image.data(),
                    image.size(), kZstdCompressionLevel);
  CHECK(!ZSTD_isError(compressed_size));
  compressed.resize(compressed_size);
  compressed.shrink_to_fit();
  return compressed;
}

// Returns |compressed| decompressed with zstd, or an empty string if
// |compressed| is empty.
std::string DecompressImage(std::string_view compressed) {
  if (compressed.empty()) {
    return std::string();
  }
  const uint64_t size =
      ZSTD_getFrameContentSize(compressed.data(), compressed.size());
  CHECK_NE(size, ZSTD_CONTENTSIZE_ERROR);
  CHECK_NE(size, ZSTD_CONTENTSIZE_UNKNOWN);
  std::string image(size, '\0');
  const size_t decompressed_size = ZSTD_decompress(
      image.data(), image.size(), compressed.data(), compressed.size());
  CHECK_EQ(decompressed_size, size);
  return image;
}

// Returns a copy of |images| with each image decompressed.
DeviceImageInfo DecompressImages(const DeviceImageInfo& images) {
  return DeviceImageInfo(DecompressImage(images.default_image()),
                         DecompressImage(images.left_bud_image()),
                         DecompressImage(images.right_bud_image()),
                         DecompressImage(images.case_image()));
}

}  // namespace

// static
constexpr char DeviceImageStore::kDeviceImageStorePref[];

// static
void DeviceImageStore::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceImageStorePref);
}

DeviceImageStore::DeviceImageStore(FastPairImageDecoder* image_decoder,
                                   PrefService* local_state)
    : image_decoder_(image_decoder), local_state_(CHECK_DEREF(local_state)) {}

DeviceImageStore::~DeviceImageStore() = default;

void DeviceImageStore::FetchDeviceImages(
    const std::string& model_id,
    DeviceMetadata* device_metadata,
    FetchDeviceImagesCallback on_images_saved_callback) {
  CHECK(device_metadata);
  std::optional<DeviceImageInfo> persisted_images;
  const DeviceImageInfo* images = nullptr;
  if (auto it = model_id_to_images_.Get(model_id);
      it != model_id_to_images_.end()) {
    images = &it->second;
  } else {
    persisted_images = GetPersistedImages(model_id);
    images = base::OptionalToPtr(persisted_images);
  }

  if ((!images || images->default_image().empty()) &&
      !device_metadata->image().IsEmpty()) {
    SaveImageAsBase64(model_id, DeviceImageType::kDefault,
                      on_images_saved_callback, device_metadata->image());
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kDefault, FetchDeviceImagesResult::kSkipped));
  }

  nearby::fastpair::TrueWirelessHeadsetImages true_wireless_images =
      device_metadata->GetDetails().true_wireless_images();

  if ((!images || images->left_bud_image().empty()) &&
      true_wireless_images.has_left_bud_url()) {
    DecodeImage(model_id, DeviceImageType::kLeftBud,
                true_wireless_images.left_bud_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kLeftBud, FetchDeviceImagesResult::kSkipped));
  }

  if ((!images || images->right_bud_image().empty()) &&
      true_wireless_images.has_right_bud_url()) {
    DecodeImage(model_id, DeviceImageType::kRightBud,
                true_wireless_images.right_bud_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kRightBud, FetchDeviceImagesResult::kSkipped));
  }

  if ((!images || images->case_image().empty()) &&
      true_wireless_images.has_case_url()) {
    DecodeImage(model_id, DeviceImageType::kCase,
                true_wireless_images.case_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kCase, FetchDeviceImagesResult::kSkipped));
  }
}

bool DeviceImageStore::PersistDeviceImages(const std::string& model_id) {
  auto it = model_id_to_images_.Peek(model_id);
  if (it == model_id_to_images_.end()) {
    // Images that are already persisted are not kept in memory.
    if (GetPersistedImages(model_id)) {
      return true;
    }
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Attempted to persist non-existent images for model ID: " +
               model_id;
    return false;
  }
  CHECK(DeviceImageInfoHasImages(it->second));

  ScopedDictPrefUpdate device_image_store(&local_state_.get(),
                                          kDeviceImageStorePref);
  if (!device_image_store->Set(
          model_id, DecompressImages(it->second).ToDictionaryValue())) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to persist images to prefs for model ID: " + model_id;
    return false;
  }
  // Prefs keep the persisted images in memory, so drop the duplicate copy.
  model_id_to_images_.Erase(it);
  return true;
}

bool DeviceImageStore::EvictDeviceImages(const std::string& model_id) {
  auto it = model_id_to_images_.Peek(model_id);
  if (it != model_id_to_images_.end()) {
    model_id_to_images_.Erase(it);
  }

  ScopedDictPrefUpdate device_image_store(&local_state_.get(),
                                          kDeviceImageStorePref);
  if (!device_image_store->Remove(model_id)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to evict images from prefs for model ID: " + model_id;
    return false;
  }
  return true;
}

std::optional<DeviceImageInfo> DeviceImageStore::GetImagesForDeviceModel(
    const std::string& model_id) {
  auto it = model_id_to_images_.Get(model_id);
  if (it != model_id_to_images_.end()) {
    return DecompressImages(it->second);
  }
  return GetPersistedImages(model_id);
}

std::optional<DeviceImageInfo> DeviceImageStore::GetPersistedImages(
    const std::string& model_id) const {
  const base::DictValue* image_dict =
      local_state_->GetDict(kDeviceImageStorePref).FindDict(model_id);
  if (!image_dict) {
    return std::nullopt;
  }
  std::optional<DeviceImageInfo> images =
      DeviceImageInfo::FromDictionaryValue(*image_dict);
  if (!images || !DeviceImageInfoHasImages(*images)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to load persisted images from prefs.";
    return std::nullopt;
  }
  return images;
}

bool DeviceImageStore::DeviceImageInfoHasImages(
    const DeviceImageInfo& images) const {
  return !images.default_image_.empty() || !images.left_bud_image_.empty() ||
         !images.right_bud_image_.empty() || !images.case_image_.empty();
}

void DeviceImageStore::DecodeImage(
    const std::string& model_id,
    DeviceImageType image_type,
    const std::string& image_url,
    FetchDeviceImagesCallback on_images_saved_callback) {
  DCHECK(image_decoder_);
  image_decoder_->DecodeImageFromUrl(
      GURL(image_url), /*resize_to_notification_size=*/true,
      base::BindOnce(&DeviceImageStore::SaveImageAsBase64,
                     weak_ptr_factory_.GetWeakPtr(), model_id, image_type,
                     std::move(on_images_saved_callback)));
}

void DeviceImageStore::SaveImageAsBase64(
    const std::string& model_id,
    DeviceImageType image_type,
    FetchDeviceImagesCallback on_images_saved_callback,
    gfx::Image image) {
  if (image.IsEmpty()) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to fetch device image.";
    std::move(on_images_saved_callback)
        .Run(std::make_pair(image_type, FetchDeviceImagesResult::kFailure));
    return;
  }

  // Select the correct path for the image.
  std::string DeviceImageInfo::* image_field = nullptr;
  if (image_type == DeviceImageType::kDefault) {
    image_field = &DeviceImageInfo::default_image_;
  } else if (image_type == DeviceImageType::kLeftBud) {
    image_field = &DeviceImageInfo::left_bud_image_;
  } else if (image_type == DeviceImageType::kRightBud) {
    image_field = &DeviceImageInfo::right_bud_image_;
  } else if (image_type == DeviceImageType::kCase) {
    image_field = &DeviceImageInfo::case_image_;
  } else {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Can't save device image to invalid image field.";
    std::move(on_images_saved_callback)
        .Run(std::make_pair(DeviceImageType::kNotSupportedType,
                            FetchDeviceImagesResult::kFailure));
    return;
  }

  // Images for a persisted model are saved directly to prefs so that they are
  // not duplicated in memory.
  auto in_memory_it = model_id_to_images_.Get(model_id);
  std::optional<DeviceImageInfo> persisted_images =
      in_memory_it != model_id_to_images_.end() ? std::nullopt
                                                : GetPersistedImages(model_id);
  if (!persisted_images && in_memory_it == model_id_to_images_.end()) {
    in_memory_it = model_id_to_images_.Put(model_id, DeviceImageInfo());
  }
  DeviceImageInfo& images =
      persisted_images ? persisted_images.value() : in_memory_it->second;

  // Encode the image as a base64 data URL. Images kept in memory are
  // compressed.
  std::string encoded_image = webui::GetBitmapDataUrl(image.AsBitmap());
  images.*image_field = persisted_images ? std::move(encoded_image)
                                         : CompressImage(encoded_image);

  if (persisted_images) {
    ScopedDictPrefUpdate device_image_store(&local_state_.get(),
                                            kDeviceImageStorePref);
    device_image_store->Set(model_id, persisted_images->ToDictionaryValue());
  }

  // Once successfully saved, return success.
  std::move(on_images_saved_callback)
      .Run(std::make_pair(image_type, FetchDeviceImagesResult::kSuccess));
}

void DeviceImageStore::RefreshCacheForTest() {
  CD_LOG(INFO, Feature::FP) << __func__;
  model_id_to_images_.Clear();
}

}  // namespace ash::quick_pair
