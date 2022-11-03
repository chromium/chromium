// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/clipboard_image_model_factory.h"

#include "base/check_op.h"

namespace ash {

namespace {

ClipboardImageModelFactory* g_instance = nullptr;

}  // namespace

ClipboardImageModelFactory::ClipboardImageModelFactory() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ClipboardImageModelFactory::~ClipboardImageModelFactory() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ClipboardImageModelFactory* ClipboardImageModelFactory::Get() {
  return g_instance;
}

}  // namespace ash
