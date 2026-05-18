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

class AuxiliarySearchDonationServiceBridge {
 public:
  static AuxiliarySearchDonationService::DonateCallback
  CreateDonationCallback();

  ~AuxiliarySearchDonationServiceBridge();

 private:
  AuxiliarySearchDonationServiceBridge();

  void DonateHistoryEntries(
      std::vector<AuxiliarySearchDonationService::HistoryData> entries) const;

  jni_zero::ScopedJavaGlobalRef<JAuxiliarySearchDonationServiceBridge> bridge_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_BRIDGE_H_
