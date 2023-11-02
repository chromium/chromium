// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_web_view_factory.h"

#include "base/check_op.h"

namespace ash {

namespace {

AshWebViewFactory* g_instance = nullptr;

}  // namespace

AshWebViewFactory::AshWebViewFactory() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AshWebViewFactory::~AshWebViewFactory() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AshWebViewFactory* AshWebViewFactory::Get() {
  return g_instance;
}

}  // namespace ash
