// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_client.h"

#include "base/check_op.h"

namespace ash {

namespace {
ProjectorClient* g_instance_ = nullptr;
}

// static
ProjectorClient* ProjectorClient::Get() {
  return g_instance_;
}

ProjectorClient::ProjectorClient() {
  DCHECK_EQ(g_instance_, nullptr);
  g_instance_ = this;
}

ProjectorClient::~ProjectorClient() {
  DCHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;
}

}  // namespace ash
