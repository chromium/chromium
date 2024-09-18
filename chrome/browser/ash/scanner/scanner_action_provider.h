// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNER_SCANNER_ACTION_PROVIDER_H_
#define CHROME_BROWSER_ASH_SCANNER_SCANNER_ACTION_PROVIDER_H_

#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"

// Responsible for interfacing with the Scanner service. This class will fetch
// any actions available from the service for the given query. It will complete
// any mapping required from the interfaces expected / returned by the service
// to the types used by the rest of the Scanner system.
class ScannerActionProvider {
 public:
  using OnActionsResolved =
      base::OnceCallback<void(ash::ScannerActionsResponse)>;

  ScannerActionProvider();
  ScannerActionProvider(const ScannerActionProvider&) = delete;
  ScannerActionProvider& operator=(const ScannerActionProvider&) = delete;
  ~ScannerActionProvider();

  // Fetches any actions available from the Scanner service. The response from
  // the service will be returned in the callback given.
  void FetchActions(OnActionsResolved callback);
};

#endif  // CHROME_BROWSER_ASH_SCANNER_SCANNER_ACTION_PROVIDER_H_
