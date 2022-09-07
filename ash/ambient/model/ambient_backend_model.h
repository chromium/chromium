// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
#define ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_

#include <string>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/containers/circular_deque.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class AmbientBackendModelObserver;

// Contains each photo image and its metadata used to show on ambient.
struct ASH_EXPORT PhotoWithDetails {
  PhotoWithDetails();

  PhotoWithDetails(const PhotoWithDetails&);
  PhotoWithDetails& operator=(const PhotoWithDetails&);
  PhotoWithDetails(PhotoWithDetails&&);
  PhotoWithDetails& operator=(PhotoWithDetails&&);

  ~PhotoWithDetails();

  void Clear();
  bool IsNull() const;

  gfx::ImageSkia photo;
  gfx::ImageSkia related_photo;
  std::string details;
  std::string related_details;
  // Hash of this image data. Used for de-duping images.
  std::string hash;
  // Whether the image is portrait or not.
  bool is_portrait = false;
  ::ambient::TopicType topic_type = ::ambient::TopicType::kOther;
};

// Stores necessary information fetched from the backdrop server to render
// the photo frame in Ambient Mode. Owned by |AmbientController|.
class ASH_EXPORT AmbientBackendModel {
 public:
  explicit AmbientBackendModel(AmbientPhotoConfig photo_config);
  AmbientBackendModel(const AmbientBackendModel&) = delete;
  AmbientBackendModel& operator=(AmbientBackendModel&) = delete;
  ~AmbientBackendModel();

  void AddObserver(AmbientBackendModelObserver* observer);
  void RemoveObserver(AmbientBackendModelObserver* observer);

  // If enough images are loaded to start ambient mode.
  bool ImagesReady() const;

  // Add image to local storage.
  void AddNextImage(const PhotoWithDetails& photo);

  // Returns true if |hash| would cause an identical image to appear twice in a
  // row. For example:
  // {A, B} + B => true
  // {A, B} + A => false
  // {A, _} + B => false
  // {A, _} + A => true
  bool IsHashDuplicate(const std::string& hash) const;

  // Record that fetching an image has failed.
  void AddImageFailure();

  void ResetImageFailures();

  bool ImageLoadingFailed();

  // Clear local storage.
  void Clear();

  // Sets the new AmbientPhotoConfig to use. This automatically |Clear()|s the
  // model of any existing topics.
  void SetPhotoConfig(AmbientPhotoConfig photo_config);

  // Returns all available decoded topics. The number of decoded topics in the
  // output will always be <= |AmbientPhotoConfig.num_decoded_topics_to_buffer|.
  //
  // Every PhotoWithDetails instance in the output shall be non-null.
  const base::circular_deque<PhotoWithDetails>& all_decoded_topics() const {
    return all_decoded_topics_;
  }

  // Gets the 2 oldest decoded topics. It's possible to accomplish this as well
  // by calling GetAllAvailableDecodedTopics() directly, but this wrapper
  // function is provided as a convenience.
  //
  // If an output PhotoWithDetails argument is nullptr, that specific topic is
  // ignored and not fetched.
  //
  // If one of the requested topics is unavailable, its corresponding output
  // argument is set to an empty PhotoWithDetails instance.
  void GetCurrentAndNextImages(PhotoWithDetails* current_image_out,
                               PhotoWithDetails* next_image_out) const;

  base::TimeDelta GetPhotoRefreshInterval() const;

  const AmbientPhotoConfig& photo_config() const { return photo_config_; }

 private:
  friend class AmbientBackendModelTest;
  friend class AmbientAshTestBase;

  void NotifyImageAdded();
  void NotifyImagesReady();
  void OnImagesReadyTimeoutFired();

  AmbientPhotoConfig photo_config_;
  std::vector<AmbientModeTopic> topics_;

  // All available decoded topics. The size of the ring buffer is capped
  // according to |AmbientPhotoConfig.num_decoded_topics_to_buffer|. The most
  // recently decoded topics are pushed to the back of the ring buffer and the
  // oldest topics are popped from the front.
  base::circular_deque<PhotoWithDetails> all_decoded_topics_;

  base::OneShotTimer images_ready_timeout_timer_;
  bool images_ready_timed_out_ = false;

  // The number of consecutive failures to load the next image.
  int failures_ = 0;

  base::ObserverList<AmbientBackendModelObserver> observers_;

  int buffer_length_for_testing_ = -1;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
