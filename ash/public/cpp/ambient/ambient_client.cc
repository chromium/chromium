// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/ambient_client.h"

#include "base/check_op.h"

namespace ash {

namespace {

AmbientClient* g_ambient_client = nullptr;

}  // namespace

// static
AmbientClient* AmbientClient::Get() {
  return g_ambient_client;
}

AmbientClient::AmbientClient() {
  DCHECK(!g_ambient_client);
  g_ambient_client = this;
}

AmbientClient::~AmbientClient() {
  DCHECK_EQ(g_ambient_client, this);
  g_ambient_client = nullptr;
}

}  // namespace ash
