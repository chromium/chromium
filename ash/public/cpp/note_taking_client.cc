// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/note_taking_client.h"

#include "base/check_op.h"

namespace ash {
namespace {
NoteTakingClient* g_note_taking_client = nullptr;
}

// static
NoteTakingClient* NoteTakingClient::GetInstance() {
  return g_note_taking_client;
}

NoteTakingClient::NoteTakingClient() {
  DCHECK(!g_note_taking_client);
  g_note_taking_client = this;
}

NoteTakingClient::~NoteTakingClient() {
  DCHECK_EQ(g_note_taking_client, this);
  g_note_taking_client = nullptr;
}

}  // namespace ash
