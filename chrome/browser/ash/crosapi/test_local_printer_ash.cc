// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"

#include "testing/gtest/include/gtest/gtest.h"

TestLocalPrinterAsh::TestLocalPrinterAsh(
    Profile* profile,
    scoped_refptr<chromeos::PpdProvider> ppd_provider)
    : profile_(profile), ppd_provider_(ppd_provider) {}

Profile* TestLocalPrinterAsh::GetProfile() {
  return profile_;
}

scoped_refptr<chromeos::PpdProvider> TestLocalPrinterAsh::CreatePpdProvider(
    Profile* profile) {
  if (!ppd_provider_) {
    ADD_FAILURE();
  }
  return ppd_provider_;
}

TestLocalPrinterAsh::~TestLocalPrinterAsh() = default;
