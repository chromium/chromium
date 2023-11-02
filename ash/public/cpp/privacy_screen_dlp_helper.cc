// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/privacy_screen_dlp_helper.h"

#include "base/check_op.h"

namespace ash {

namespace {
PrivacyScreenDlpHelper* g_instance = nullptr;
}  // namespace

// static
PrivacyScreenDlpHelper* PrivacyScreenDlpHelper::Get() {
  DCHECK(g_instance);
  return g_instance;
}

PrivacyScreenDlpHelper::PrivacyScreenDlpHelper() {
  DCHECK(!g_instance);
  g_instance = this;
}

PrivacyScreenDlpHelper::~PrivacyScreenDlpHelper() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
