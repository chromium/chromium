// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/android_intent_helper.h"
#include "base/logging.h"

namespace ash {
namespace {

AndroidIntentHelper* g_android_intent_helper = nullptr;

// Scheme of the Android intent url.
constexpr char kAndroidIntentScheme[] = "intent";

// Prefix of the Android intent ref fragment.
constexpr char kAndroidIntentPrefix[] = "Intent;";

}  // namespace

// static
AndroidIntentHelper* AndroidIntentHelper::GetInstance() {
  return g_android_intent_helper;
}

AndroidIntentHelper::AndroidIntentHelper() {
  DCHECK(!g_android_intent_helper);
  g_android_intent_helper = this;
}

AndroidIntentHelper::~AndroidIntentHelper() {
  DCHECK_EQ(g_android_intent_helper, this);
  g_android_intent_helper = nullptr;
}

bool IsAndroidIntent(const GURL& url) {
  return url.SchemeIs(kAndroidIntentScheme) ||
         base::StartsWith(url.ref(), kAndroidIntentPrefix,
                          base::CompareCase::SENSITIVE);
}

}  // namespace ash
