// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/management_disclosure_client.h"

#include "base/check_op.h"

namespace ash {

namespace {

ManagementDisclosureClient* g_client = nullptr;

}  // namespace

ManagementDisclosureClient::ManagementDisclosureClient() {
  DCHECK(!g_client);
  g_client = this;
}

ManagementDisclosureClient::~ManagementDisclosureClient() {
  DCHECK_EQ(g_client, this);
  g_client = nullptr;
}

}  // namespace ash
