// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include <utility>
#include <vector>

#include "chrome/browser/tab/protocol/tab_state.pb.h"

namespace tabs {

TabStoragePackage::TabStoragePackage(tabs_pb::TabState tab_state)
    : tab_state_(std::move(tab_state)) {}

TabStoragePackage::~TabStoragePackage() = default;

std::vector<uint8_t> TabStoragePackage::SerializePayload() const {
  std::vector<uint8_t> payload_vec(tab_state_.ByteSizeLong());
  tab_state_.SerializeToArray(payload_vec.data(), payload_vec.size());
  return payload_vec;
}

std::vector<uint8_t> TabStoragePackage::SerializeChildren() const {
  return {};
}

}  // namespace tabs
