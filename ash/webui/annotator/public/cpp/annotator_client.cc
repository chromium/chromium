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
  DCHECK(g_instance);
  return g_instance;
}

AnnotatorClient::AnnotatorClient() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AnnotatorClient::~AnnotatorClient() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace ash
