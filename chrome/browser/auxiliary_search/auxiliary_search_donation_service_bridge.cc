// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service_bridge.h"

#include <jni.h>

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"
#include "third_party/jni_zero/jni_zero.h"

// `ToJniType` specialisation declarations:
#include "base/android/jni_string.h"  // IWYU pragma: keep

namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject>
ToJniType<AuxiliarySearchDonationService::HistoryData>(
    JNIEnv* env,
    const AuxiliarySearchDonationService::HistoryData& data);

}  // namespace jni_zero

// Must come after all `ToJniType` specialisation declarations.
#include "chrome/browser/auxiliary_search/jni_headers/AuxiliarySearchDonationServiceBridge_jni.h"

namespace jni_zero {

template <>
ScopedJavaLocalRef<jobject>
ToJniType<AuxiliarySearchDonationService::HistoryData>(
    JNIEnv* env,
    const AuxiliarySearchDonationService::HistoryData& data) {
  return AuxiliarySearchDonationServiceBridgeJni::createHistoryDocument(
      env, data.url_key, data.url.spec(), data.title,
      data.last_visited.InMillisecondsSinceUnixEpoch());
}

}  // namespace jni_zero

// static
AuxiliarySearchDonationService::DonateCallback
AuxiliarySearchDonationServiceBridge::CreateDonationCallback() {
  return base::BindRepeating(
      &AuxiliarySearchDonationServiceBridge::DonateHistoryEntries,
      base::Owned(new AuxiliarySearchDonationServiceBridge()));
}

AuxiliarySearchDonationServiceBridge::AuxiliarySearchDonationServiceBridge()
    : bridge_(AuxiliarySearchDonationServiceBridgeJni::New(
          base::android::AttachCurrentThread())) {}
AuxiliarySearchDonationServiceBridge::~AuxiliarySearchDonationServiceBridge() =
    default;

void AuxiliarySearchDonationServiceBridge::DonateHistoryEntries(
    std::vector<AuxiliarySearchDonationService::HistoryData> entries) const {
  // As of writing, `jni_zero` generated functions take in arguments as
  // `const&`, so the following `std::move` is a no-op.
  // If `jni_zero` ever changes its behaviour to allow passing in arguments by
  // move, this will automatically take advantage of that.
  bridge_->donateHistory(base::android::AttachCurrentThread(),
                         std::move(entries));
}
