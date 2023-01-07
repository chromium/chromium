// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_session.h"

#include "base/check_op.h"

namespace ash {

namespace {
ProjectorSession* g_instance = nullptr;
}

ProjectorSession::ProjectorSession() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ProjectorSession::~ProjectorSession() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ProjectorSession* ProjectorSession::Get() {
  return g_instance;
}

}  // namespace ash