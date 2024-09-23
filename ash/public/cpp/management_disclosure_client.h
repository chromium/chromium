// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_
#define ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_

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

  virtual void SetVisible(bool visible) = 0;

 protected:
  virtual ~ManagementDisclosureClient();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MANAGEMENT_DISCLOSURE_CLIENT_H_
