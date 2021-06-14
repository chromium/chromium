// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/core/share_targets.h"

#include <string>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/share/proto/share_target.pb.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"

namespace sharing {

using base::AutoLock;

static const char GLOBAL[] = "GLOBAL";

// Our Singleton needs to populate itself when first constructed.
// This is left out of the constructor to make testing simpler.
struct ShareTargetsSingletonTrait
    : public base::DefaultSingletonTraits<ShareTargets> {
  static ShareTargets* New() {
    ShareTargets* instance = new ShareTargets();
    // TODO PopulateFromResourceBundle
    return instance;
  }
};

// --- ShareTargets methods ---

// static
ShareTargets* ShareTargets::GetInstance() {
  return base::Singleton<ShareTargets, ShareTargetsSingletonTrait>::get();
}

ShareTargets::ShareTargets() = default;

ShareTargets::~ShareTargets() {
  AutoLock lock(lock_);  // DCHECK fail if the lock is held.
}

void ShareTargets::RecordUpdateMetrics(UpdateResult result,
                                       const std::string& src_name) {
  lock_.AssertAcquired();

  // TODO record histograms
}

void ShareTargets::PopulateFromDynamicUpdate(const std::string& binary_pb) {
  AutoLock lock(lock_);
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, "DynamicUpdate");
}

ShareTargets::UpdateResult ShareTargets::PopulateFromBinaryPb(
    const std::string& binary_pb) {
  lock_.AssertAcquired();

  // Parse the proto and do some validation on it.
  if (binary_pb.empty()) {
    return UpdateResult::FAILED_EMPTY;
  }

  std::unique_ptr<mojom::MapLocaleTargets> new_targets(
      new mojom::MapLocaleTargets);

  if (!new_targets->ParseFromString(binary_pb)) {
    return UpdateResult::FAILED_PROTO_PARSE;
  }

  // Compare against existing targets, if we have one.
  if (targets_) {
    // If versions are equal, we skip the update but it's not really
    // a failure.
    if (new_targets->version_id() == targets_->version_id())
      return UpdateResult::SKIPPED_VERSION_CHECK_EQUAL;

    // Check that version number increases
    if (new_targets->version_id() <= targets_->version_id())
      return UpdateResult::FAILED_VERSION_CHECK;
  }

  // Looks good. Update our internal list.
  SwapTargetsLocked(new_targets);

  return UpdateResult::SUCCESS;
}

void ShareTargets::SwapTargetsLocked(
    std::unique_ptr<mojom::MapLocaleTargets>& new_targets) {
  lock_.AssertAcquired();
  targets_.swap(new_targets);
}

// Return the ShareTarget for a given locale. If the locale is not found return
// a global locale.
const mojom::ShareTargets* ShareTargets::GetShareTargetsForLocale(
    const std::string& locale) {
  // TODO: ensure correct thread is used in order to call
  // lock_.AssertAcquired();
  if (!targets_ || targets_->map_target_locale_map().size() == 0) {
    return nullptr;
  }

  auto it = targets_->map_target_locale_map().find(locale);

  if (it == targets_->map_target_locale_map().end()) {
    it = targets_->map_target_locale_map().find(GLOBAL);
  }

  if (it == targets_->map_target_locale_map().end()) {
    return nullptr;
  }

  return &(it->second);
}
}  // namespace sharing
