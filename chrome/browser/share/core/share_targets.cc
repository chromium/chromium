// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/core/share_targets.h"

#include <string>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/share/core/share_targets_observer.h"
#include "chrome/browser/share/proto/share_target.pb.h"
#include "chrome/grit/browser_resources.h"
#include "components/country_codes/country_codes.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl.h"
#include "ui/base/resource/resource_bundle.h"

namespace sharing {

static const char GLOBAL[] = "GLOBAL";

// Our Singleton needs to populate itself when first constructed.
// This is left out of the constructor to make testing simpler.
struct ShareTargetsSingletonTrait
    : public base::DefaultSingletonTraits<ShareTargets> {
  static ShareTargets* New() {
    ShareTargets* instance = new ShareTargets();
    instance->PopulateFromResourceBundle();
    return instance;
  }
};

// --- ShareTargets methods ---

// static
ShareTargets* ShareTargets::GetInstance() {
  return base::Singleton<ShareTargets, ShareTargetsSingletonTrait>::get();
}

ShareTargets::ShareTargets() = default;
ShareTargets::~ShareTargets() = default;

void ShareTargets::RecordUpdateMetrics(UpdateResult result, UpdateOrigin src) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // src_name should be "ResourceBundle" or "DynamicUpdate".
  if (src == UpdateOrigin::DYNAMIC_UPDATE) {
    UMA_HISTOGRAM_ENUMERATION("Sharing.ShareTargetUpdate.DynamicUpdateResult",
                              result);
    if (result == UpdateResult::SUCCESS) {
      UMA_HISTOGRAM_COUNTS_1000(
          "Sharing.ShareTargetUpdate.DynamicUpdateVersion",
          targets_->version_id());
    }
  } else if (src == UpdateOrigin::RESOURCE_BUNDLE) {
    UMA_HISTOGRAM_ENUMERATION("Sharing.ShareTargetUpdate.ResourceBundleResult",
                              result);
  }
}

void ShareTargets::PopulateFromDynamicUpdate(const std::string& binary_pb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, UpdateOrigin::DYNAMIC_UPDATE);
}

void ShareTargets::PopulateFromResourceBundle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::string binary_pb =
      bundle.LoadDataResourceString(IDR_DESKTOP_SHARING_HUB_PB);
  UpdateResult result = PopulateFromBinaryPb(binary_pb);
  RecordUpdateMetrics(result, UpdateOrigin::RESOURCE_BUNDLE);
}

ShareTargets::UpdateResult ShareTargets::PopulateFromBinaryPb(
    const std::string& binary_pb) {
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
  SwapTargets(new_targets);
  NotifyShareTargetUpdated();
  return UpdateResult::SUCCESS;
}

void ShareTargets::SwapTargets(
    std::unique_ptr<mojom::MapLocaleTargets>& new_targets) {
  targets_.swap(new_targets);
}

void ShareTargets::AddObserver(ShareTargetsObserver* observer) {
  observers_.AddObserver(observer);
  if (targets_) {
    NotifyObserver(observer);
  }
}

void ShareTargets::RemoveObserver(ShareTargetsObserver* observer) {
  observers_.RemoveObserver(observer);
}

std::string ShareTargets::GetCountryStringFromID(int countryID) {
  // Decode the country code string from the provided integer.
  unsigned char mask = 0xFF;
  char c2 = static_cast<char>(mask & countryID);
  char c1 = static_cast<char>(countryID >> 8);
  return std::string() + static_cast<char>(toupper(c1)) +
         static_cast<char>(toupper(c2));
}

void ShareTargets::NotifyObserver(ShareTargetsObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string locale =
      GetCountryStringFromID(country_codes::GetCurrentCountryID());

  auto it = targets_->map_target_locale_map().find(locale);

  if (it == targets_->map_target_locale_map().end()) {
    it = targets_->map_target_locale_map().find(GLOBAL);
  }
  std::unique_ptr<mojom::ShareTargets> to_return(new mojom::ShareTargets());
  to_return->CopyFrom(it->second);
  observer->OnShareTargetsUpdated(std::move(to_return));
}

void ShareTargets::NotifyShareTargetUpdated() {
  if (!targets_)
    return;
  for (ShareTargetsObserver& observer : observers_) {
    NotifyObserver(&observer);
  }
}

}  // namespace sharing
