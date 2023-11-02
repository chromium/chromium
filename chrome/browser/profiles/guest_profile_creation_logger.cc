// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/guest_profile_creation_logger.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"

namespace profile {

namespace {

const void* const kGuestProfileCreationLoggerKey =
    &kGuestProfileCreationLoggerKey;

const char kGuestTypeCreatedHistogramName[] = "Profile.Guest.TypeCreated";

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "GuestProfileCreatedType" in src/tools/metrics/histograms/enums.xml.
enum class GuestProfileCreatedType {
  kParentGuest = 0,
  kFirstChildGuest = 1,
  kMaxValue = kFirstChildGuest,
};

class GuestProfileCreationLogger : public base::SupportsUserData::Data {
 public:
  GuestProfileCreationLogger() = default;
  GuestProfileCreationLogger(const GuestProfileCreationLogger&) = delete;
  GuestProfileCreationLogger& operator=(const GuestProfileCreationLogger&) =
      delete;

  void RecordParentCreation() {
    DCHECK(!first_child_guest_recorded_);

    base::UmaHistogramEnumeration(kGuestTypeCreatedHistogramName,
                                  GuestProfileCreatedType::kParentGuest);
  }

  void MaybeRecordChildCreation() {
    if (first_child_guest_recorded_)
      return;

    base::UmaHistogramEnumeration(kGuestTypeCreatedHistogramName,
                                  GuestProfileCreatedType::kFirstChildGuest);

    first_child_guest_recorded_ = true;
  }

 private:
  bool first_child_guest_recorded_ = false;
};

}  // namespace

void RecordGuestParentCreation(Profile* profile) {
  DCHECK(profile->IsGuestSession());
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(!profile->GetUserData(kGuestProfileCreationLoggerKey));

  auto logger = std::make_unique<GuestProfileCreationLogger>();
  logger->RecordParentCreation();
  profile->SetUserData(kGuestProfileCreationLoggerKey, std::move(logger));
}

void MaybeRecordGuestChildCreation(Profile* profile) {
  DCHECK(profile->IsGuestSession());
  DCHECK(profile->IsOffTheRecord());

  auto* raw_data = profile->GetOriginalProfile()->GetUserData(
      kGuestProfileCreationLoggerKey);
  DCHECK(raw_data);

  static_cast<GuestProfileCreationLogger*>(raw_data)
      ->MaybeRecordChildCreation();
}

}  // namespace profile
