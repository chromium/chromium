// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/net/browser_url_opener.h"

#include "base/logging.h"

namespace arc {

namespace {

BrowserUrlOpener* g_instance = nullptr;

}  // namespace

// static
BrowserUrlOpener* BrowserUrlOpener::Get() {
  DCHECK(g_instance);
  return g_instance;
}

BrowserUrlOpener::BrowserUrlOpener() {
  if (g_instance) {
    LOG(ERROR)
        << "Overwriting g_instance. This should not happen except for testing.";
  }
  g_instance = this;
}

BrowserUrlOpener::~BrowserUrlOpener() {
  if (g_instance != this) {
    LOG(ERROR) << "g_instance was not properly set. This should not happen "
                  "except for testing.";
  }
  g_instance = nullptr;
}

}  // namespace arc
