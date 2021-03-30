// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/chrome_test_util_jni_headers/CustomTabsTestUtils_jni.h"
#include "components/variations/variations_ids_provider.h"

namespace customtabs {

static jboolean JNI_CustomTabsTestUtils_HasVariationId(JNIEnv* env, jint id) {
  auto ids = variations::VariationsIdsProvider::GetInstance()
                 ->GetVariationsVectorForWebPropertiesKeys();
  return base::Contains(ids, id);
}

}  // namespace customtabs
