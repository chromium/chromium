// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/device_image_store.h"

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/shell.h"
#include "base/values.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace ash::quick_pair {

// Alias DeviceImageInfo for convenience.
using bluetooth_config::DeviceImageInfo;

// static
constexpr char DeviceImageStore::kDeviceImageStorePref[];

// static
void DeviceImageStore::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kDeviceImageStorePref);
}

DeviceImageStore::DeviceImageStore(FastPairImageDecoder* image_decoder)
    : image_decoder_(image_decoder) {}

DeviceImageStore::~DeviceImageStore() = default;

void DeviceImageStore::FetchDeviceImages(
    const std::string& model_id,
    DeviceMetadata* device_metadata,
    FetchDeviceImagesCallback on_images_saved_callback) {
  DCHECK(device_metadata);
  DeviceImageInfo& images = model_id_to_images_[model_id];

  if (images.default_image().empty() && !device_metadata->image().IsEmpty()) {
    SaveImageAsBase64(model_id, DeviceImageType::kDefault,
                      on_images_saved_callback, device_metadata->image());
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kDefault, FetchDeviceImagesResult::kSkipped));
  }

  nearby::fastpair::TrueWirelessHeadsetImages true_wireless_images =
      device_metadata->GetDetails().true_wireless_images();

  if (images.left_bud_image().empty() &&
      true_wireless_images.has_left_bud_url()) {
    DecodeImage(model_id, DeviceImageType::kLeftBud,
                true_wireless_images.left_bud_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kLeftBud, FetchDeviceImagesResult::kSkipped));
  }

  if (images.right_bud_image().empty() &&
      true_wireless_images.has_right_bud_url()) {
    DecodeImage(model_id, DeviceImageType::kRightBud,
                true_wireless_images.right_bud_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kRightBud, FetchDeviceImagesResult::kSkipped));
  }

  if (images.case_image().empty() && true_wireless_images.has_case_url()) {
    DecodeImage(model_id, DeviceImageType::kCase,
                true_wireless_images.case_url(), on_images_saved_callback);
  } else {
    on_images_saved_callback.Run(std::make_pair(
        DeviceImageType::kCase, FetchDeviceImagesResult::kSkipped));
  }
}

bool DeviceImageStore::PersistDeviceImages(const std::string& model_id) {
  DeviceImageInfo& images = model_id_to_images_[model_id];

  if (!DeviceImageInfoHasImages(images)) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Attempted to persist non-existent images for model ID: " +
               model_id;
    return false;
  }
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return false;
  }
  ScopedDictPrefUpdate device_image_store(local_state, kDeviceImageStorePref);
  // TODO(dclasson): Once we add TrueWireless support, need to modify this to
  // merge new & persisted images objects.
  if (!device_image_store->Set(model_id, images.ToDictionaryValue())) {
    CD_LOG(WARNING, Feature::FP)
        << __func__
        << ": Failed to persist images to prefs for model ID: " + model_id;
    return false;
  }
  return true;
}

bool DeviceImageStore::EvictDeviceImages(const std::string& model_id) {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return false;
  }
  ScopedDictPrefUpdate device_image_store(local_state, kDeviceImageStorePref);
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
  // Lazily load saved images from prefs the first time we get an image.
  if (!loaded_images_from_prefs_) {
    loaded_images_from_prefs_ = true;
    LoadPersistedImagesFromPrefs();
  }

  DeviceImageInfo& images = model_id_to_images_[model_id];

  if (!DeviceImageInfoHasImages(images)) {
    return std::nullopt;
  }
  return images;
}

void DeviceImageStore::LoadPersistedImagesFromPrefs() {
  PrefService* local_state = Shell::Get()->local_state();
  if (!local_state) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": No shell local state available.";
    return;
  }
  const base::Value::Dict& device_image_store =
      local_state->GetDict(kDeviceImageStorePref);
  for (auto [model_id, image_dict] : device_image_store) {
    std::optional<DeviceImageInfo> images;
    if (image_dict.is_dict()) {
      images = DeviceImageInfo::FromDictionaryValue(image_dict.GetDict());
    }
    if (!images) {
      CD_LOG(WARNING, Feature::FP)
          << __func__ << ": Failed to load persisted images from prefs.";
      continue;
    }
    model_id_to_images_[model_id] = images.value();
  }
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

  // Encode the image as a base64 data URL.
  std::string encoded_image = webui::GetBitmapDataUrl(image.AsBitmap());

  // Save the image in the correct path.
  if (image_type == DeviceImageType::kDefault) {
    model_id_to_images_[model_id].default_image_ = encoded_image;
  } else if (image_type == DeviceImageType::kLeftBud) {
    model_id_to_images_[model_id].left_bud_image_ = encoded_image;
  } else if (image_type == DeviceImageType::kRightBud) {
    model_id_to_images_[model_id].right_bud_image_ = encoded_image;
  } else if (image_type == DeviceImageType::kCase) {
    model_id_to_images_[model_id].case_image_ = encoded_image;
  } else {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Can't save device image to invalid image field.";
    std::move(on_images_saved_callback)
        .Run(std::make_pair(DeviceImageType::kNotSupportedType,
                            FetchDeviceImagesResult::kFailure));
    return;
  }

  // Once successfully saved, return success.
  std::move(on_images_saved_callback)
      .Run(std::make_pair(image_type, FetchDeviceImagesResult::kSuccess));
}

void DeviceImageStore::RefreshCacheForTest() {
  CD_LOG(INFO, Feature::FP) << __func__;
  model_id_to_images_.clear();
  LoadPersistedImagesFromPrefs();
}

}  // namespace ash::quick_pair
