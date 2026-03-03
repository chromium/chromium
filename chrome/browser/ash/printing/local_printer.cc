// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/local_printer.h"

#include "base/check_is_test.h"

namespace ash {

namespace {

LocalPrinter* g_instance = nullptr;

}  // namespace

bool LocalPrinter::IsSet() {
  return g_instance != nullptr;
}

LocalPrinter* LocalPrinter::Get() {
  CHECK(g_instance);
  return g_instance;
}

LocalPrinter::LocalPrinter() {
  CHECK(!g_instance);
  g_instance = this;
}

LocalPrinter::~LocalPrinter() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
