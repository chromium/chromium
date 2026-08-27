// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/offline_items_collection/core/fail_state.h"
#include "ui/base/l10n/l10n_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/StringUtils_jni.h"

static std::u16string JNI_StringUtils_GetFailStateMessage(
    offline_items_collection::FailState fail_state) {
  std::u16string message = OfflineItemUtils::GetFailStateMessage(fail_state);
  return l10n_util::GetStringFUTF16(IDS_DOWNLOAD_STATUS_INTERRUPTED, message);
}

DEFINE_JNI(StringUtils)
