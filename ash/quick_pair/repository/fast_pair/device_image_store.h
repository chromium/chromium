// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_IMAGE_STORE_H_
#define ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_IMAGE_STORE_H_

#include <optional>
#include <string>
#include <vector>

#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "ui/gfx/image/image.h"

class PrefRegistrySimple;

namespace ash {
namespace quick_pair {

class FastPairImageDecoder;

// Saves any discovered device images in a flat_map model_id_to_images_.
// Images are saved in DeviceImageInfo objects and are loaded from prefs on
// creation. Images can be persisted to prefs (i.e. on device pair) or
// evicted from prefs (i.e. on device unpair). Images can be retrieved
// given a model_id.
class DeviceImageStore {
 public:
  static constexpr char kDeviceImageStorePref[] =
      "fast_pair.device_image_store";

  // Corresponds to the types of device images that we currently support. Used
  // to keep track of pending downloads.
  // kNotSupportedType is an error state that signifies that the image is not a
  // supported type.
  enum class DeviceImageType {
    kNotSupportedType = 0,
    kDefault = 1,
    kLeftBud = 2,
    kRightBud = 3,
    kCase = 4
  };

  // Corresponds to the status of a FetchDeviceImages call.
  enum class FetchDeviceImagesResult {
    kSuccess = 0,
    kFailure = 1,
    // Skipped refers to when an image was already saved or there is no matching
    // URL to attempt a download.
    kSkipped = 2
  };

  // Returns the type of image that was was fetched and the result, i.e.
  // DeviceImageType::kDefault and FetchDeviceImagesResult::kSuccess after
  // successfully saving a default image.
  using FetchDeviceImagesCallback = base::RepeatingCallback<void(
      std::pair<DeviceImageType, FetchDeviceImagesResult>)>;

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DeviceImageStore(FastPairImageDecoder* image_decoder);
  DeviceImageStore(const DeviceImageStore&) = delete;
  DeviceImageStore& operator=(const DeviceImageStore&) = delete;
  ~DeviceImageStore();

  // Saves the device images stored in |device_metadata| to model_id_to_images_,
  // mapped to by |model_id|, if there are images.
  void FetchDeviceImages(const std::string& model_id,
                         DeviceMetadata* device_metadata,
                         FetchDeviceImagesCallback on_images_saved_callback);

  // Persists the DeviceImageInfo for |model_id| in model_id_to_images_
  // to local state prefs. Returns true if images were persisted, false
  // if |model_id| has no saved images or there was an error when persisting.
  bool PersistDeviceImages(const std::string& model_id);

  // Evicts the DeviceImageInfo corresponding to |model_id| in
  // model_id_to_images_ from local state prefs. Returns true if |model_id| is
  // evicted from prefs, false otherwise.
  bool EvictDeviceImages(const std::string& model_id);

  // Returns a DeviceImageInfo of device images belonging to |model_id|, if
  // found.
  std::optional<bluetooth_config::DeviceImageInfo> GetImagesForDeviceModel(
      const std::string& model_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest, PersistDeviceImages);
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest,
                           PersistDeviceImagesNoMacAddress);
  FRIEND_TEST_ALL_PREFIXES(FastPairRepositoryImplTest, EvictDeviceImages);

  // Loads device images stored in prefs to model_id_to_images_.
  void LoadPersistedImagesFromPrefs();

  // Returns true if |images| contains at least one image, false otherwise.
  bool DeviceImageInfoHasImages(
      const bluetooth_config::DeviceImageInfo& images) const;

  // Wrapper around a call to FastPairImageDecoder's DecodeImage. Downloads
  // and decodes the image at |image_url|, then passes the |model_id|,
  // |image_type|, and decoded image to SaveImageAsBase64.
  void DecodeImage(const std::string& model_id,
                   DeviceImageType image_type,
                   const std::string& image_url,
                   FetchDeviceImagesCallback on_images_saved_callback);

  // Callee ensures |image| is not empty. Encodes |image| as a base64 data URL
  // and saves it to the DeviceImageInfo belonging to |model_id| in field
  // |image_type|. Invokes |on_images_saved_callback| with the
  // |image_type| where the image was saved on success, empty string
  // otherwise.
  void SaveImageAsBase64(const std::string& model_id,
                         DeviceImageType image_type,
                         FetchDeviceImagesCallback on_images_saved_callback,
                         gfx::Image image);

  // Clears the in-memory map and reloads from prefs. Used by tests.
  void RefreshCacheForTest();

  // Maps from model IDs to images stored in DeviceImageInfo.
  base::flat_map<std::string, bluetooth_config::DeviceImageInfo>
      model_id_to_images_;
  // Used to lazily load images from prefs.
  bool loaded_images_from_prefs_ = false;
  raw_ptr<FastPairImageDecoder> image_decoder_;
  base::WeakPtrFactory<DeviceImageStore> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_FAST_PAIR_DEVICE_IMAGE_STORE_H_
