// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/fwupd_download_client.h"

#include "base/check_op.h"

namespace ash {

namespace {

FwupdDownloadClient* g_client = nullptr;

}  // namespace

// static
FwupdDownloadClient* FwupdDownloadClient::Get() {
  return g_client;
}

FwupdDownloadClient::FwupdDownloadClient() {
  DCHECK(!g_client);
  g_client = this;
}

FwupdDownloadClient::~FwupdDownloadClient() {
  DCHECK_EQ(g_client, this);
  g_client = nullptr;
}

}  // namespace ash
