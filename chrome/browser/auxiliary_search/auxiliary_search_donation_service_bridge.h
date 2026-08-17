// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_BRIDGE_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_BRIDGE_H_

#include <jni.h>

#include <vector>

#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"
#include "chrome/browser/auxiliary_search/jni_headers/AuxiliarySearchDonationServiceBridge_shared_jni.h"
#include "third_party/jni_zero/jni_zero.h"

struct CoreAccountInfo;

class AuxiliarySearchDonationServiceBridge
    : public AuxiliarySearchDonationService::Delegate {
 public:
  static bool IsBrowsingDataDonationSupported();

  explicit AuxiliarySearchDonationServiceBridge(
      bool is_browsing_data_donation_enabled);
  ~AuxiliarySearchDonationServiceBridge() override;

  void DonateHistoryEntries(
      std::vector<AuxiliarySearchDonationService::HistoryData> entries,
      CoreAccountInfo account_info) override;
  void SetBrowsingDataDonationEnabled(
      bool is_browsing_data_donation_enabled) override;

 private:
  jni_zero::ScopedJavaGlobalRef<JAuxiliarySearchDonationServiceBridge> bridge_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_BRIDGE_H_
