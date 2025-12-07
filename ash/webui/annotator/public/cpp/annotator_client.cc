// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/annotator/public/cpp/annotator_client.h"

#include "base/check_op.h"

namespace ash {

namespace {
AnnotatorClient* g_instance = nullptr;
}  // namespace

// static
AnnotatorClient* AnnotatorClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

// static
bool AnnotatorClient::HasInstance() {
  return g_instance != nullptr;
}

AnnotatorClient::AnnotatorClient() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AnnotatorClient::~AnnotatorClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
