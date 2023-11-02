// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"

namespace media_router {

ParsedDialDeviceDescription::ParsedDialDeviceDescription() = default;

ParsedDialDeviceDescription::ParsedDialDeviceDescription(
    const ParsedDialDeviceDescription& other) = default;
ParsedDialDeviceDescription::~ParsedDialDeviceDescription() = default;

ParsedDialDeviceDescription& ParsedDialDeviceDescription::operator=(
    const ParsedDialDeviceDescription& other) {
  if (this == &other)
    return *this;

  this->unique_id = other.unique_id;
  this->friendly_name = other.friendly_name;
  this->model_name = other.model_name;
  this->device_type = other.device_type;
  this->app_url = other.app_url;

  return *this;
}

bool ParsedDialDeviceDescription::operator==(
    const ParsedDialDeviceDescription& other) const {
  return unique_id == other.unique_id && friendly_name == other.friendly_name &&
         model_name == other.model_name && device_type == other.device_type &&
         app_url == other.app_url;
}

}  // namespace media_router
