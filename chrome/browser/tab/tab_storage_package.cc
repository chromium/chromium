// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_storage_package.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"

namespace tabs {

TabStoragePackage::TabStoragePackage(
    tabs_pb::TabState tab_state,
    std::unique_ptr<base::Pickle> web_contents_state_pickle)
    : tab_state_(std::move(tab_state)),
      web_contents_state_pickle_(std::move(web_contents_state_pickle)) {
  CHECK(!web_contents_state_pickle_ ||
        !tab_state_.has_web_contents_state_bytes());
}

TabStoragePackage::~TabStoragePackage() = default;

std::vector<uint8_t> TabStoragePackage::SerializePayload() const {
  if (web_contents_state_pickle_) {
    tab_state_.set_web_contents_state_bytes(
        web_contents_state_pickle_->AsStringView());
    web_contents_state_pickle_.reset();
  }
  std::vector<uint8_t> payload(tab_state_.ByteSizeLong());
  CHECK(tab_state_.SerializeToArray(payload.data(),
                                    base::checked_cast<int>(payload.size())));
  return payload;
}

std::vector<uint8_t> TabStoragePackage::SerializeChildren() const {
  return {};
}

}  // namespace tabs
