// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
#define ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
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
};

// Stores necessary information fetched from the backdrop server to render
// the photo frame and glanceable weather information on Ambient Mode. Owned
// by |AmbientController|.
class ASH_EXPORT AmbientBackendModel {
 public:
  AmbientBackendModel();
  AmbientBackendModel(const AmbientBackendModel&) = delete;
  AmbientBackendModel& operator=(AmbientBackendModel&) = delete;
  ~AmbientBackendModel();

  void AddObserver(AmbientBackendModelObserver* observer);
  void RemoveObserver(AmbientBackendModelObserver* observer);

  void AppendTopics(const std::vector<AmbientModeTopic>& topics);
  const std::vector<AmbientModeTopic>& topics() const { return topics_; }

  // Prefetch one more image for ShowNextImage animations.
  bool ShouldFetchImmediately() const;

  // Add image to local storage.
  void AddNextImage(const PhotoWithDetails& photo);

  // Get/Set the photo refresh interval.
  base::TimeDelta GetPhotoRefreshInterval();
  void SetPhotoRefreshInterval(base::TimeDelta interval);

  // Clear local storage.
  void Clear();

  // Get images from local storage. Could be null image.
  const PhotoWithDetails& GetNextImage() const;

  // Updates the weather information and notifies observers if the icon image is
  // not null.
  void UpdateWeatherInfo(const gfx::ImageSkia& weather_condition_icon,
                         float temperature_fahrenheit,
                         bool show_celsius);

  // Returns the cached condition icon. Will return a null image if it has not
  // been set yet.
  const gfx::ImageSkia& weather_condition_icon() const {
    return weather_condition_icon_;
  }

  // Returns the cached temperature value in Fahrenheit.
  float temperature_fahrenheit() const { return temperature_fahrenheit_; }

  // Calculate the temperature in celsius.
  float GetTemperatureInCelsius() const;

  bool show_celsius() const { return show_celsius_; }

 private:
  friend class AmbientBackendModelTest;

  void NotifyTopicsChanged();
  void NotifyImagesChanged();
  void NotifyWeatherInfoUpdated();

  std::vector<AmbientModeTopic> topics_;

  // Local cache of downloaded images for photo transition animation.
  PhotoWithDetails current_image_;
  PhotoWithDetails next_image_;

  // The index of currently shown image.
  int current_image_index_ = 0;

  // Current weather information.
  gfx::ImageSkia weather_condition_icon_;
  float temperature_fahrenheit_ = 0.0f;
  bool show_celsius_ = false;

  // The interval to refresh photos.
  base::TimeDelta photo_refresh_interval_;

  base::ObserverList<AmbientBackendModelObserver> observers_;

  int buffer_length_for_testing_ = -1;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_H_
