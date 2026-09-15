// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_HISTORY_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_HISTORY_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/history/history_service_provider.h"

class AccountId;

namespace history {
class HistoryService;
}  // namespace history

namespace ash {

// //chrome-side implementation of HistoryServiceProvider. Wraps
// //chrome/browser/history's Profile-keyed HistoryServiceFactory so ChromeOS
// callers can reach the service through the chromeos-side interface.
class HistoryServiceProviderImpl : public HistoryServiceProvider {
 public:
  HistoryServiceProviderImpl();
  HistoryServiceProviderImpl(const HistoryServiceProviderImpl&) = delete;
  HistoryServiceProviderImpl& operator=(const HistoryServiceProviderImpl&) =
      delete;
  ~HistoryServiceProviderImpl() override;

  // HistoryServiceProvider:
  history::HistoryService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_HISTORY_SERVICE_PROVIDER_IMPL_H_
