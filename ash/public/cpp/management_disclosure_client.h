// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_
#define ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An interface that allows Ash to trigger management disclosure which Chrome is
// responsible for.
class ASH_PUBLIC_EXPORT ManagementDisclosureClient {
 public:
  ManagementDisclosureClient();
  ManagementDisclosureClient(const ManagementDisclosureClient&) = delete;
  ManagementDisclosureClient& operator=(const ManagementDisclosureClient&) =
      delete;

  // Retrieves the list of device policy disclosures from the
  // management_ui_handler (same place chrome://management is populated from so
  // they should match). The device disclosures are than passed to
  // management_disclosure_dialog so they can be shown on the login/lock screen.
  virtual std::vector<std::u16string> GetDisclosures() = 0;

 protected:
  virtual ~ManagementDisclosureClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_
