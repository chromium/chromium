// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/variations/variations_ids_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_test_util_jni/CustomTabsTestUtils_jni.h"

namespace customtabs {

static bool JNI_CustomTabsTestUtils_HasVariationId(JNIEnv* env, int32_t id) {
  auto ids = variations::VariationsIdsProvider::GetInstance()
                 ->GetVariationsVectorForWebPropertiesKeys();
  return std::ranges::contains(ids, id);
}

}  // namespace customtabs

DEFINE_JNI(CustomTabsTestUtils)
