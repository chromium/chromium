// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "components/variations/variations_ids_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_test_util_jni/CustomTabsTestUtils_jni.h"

namespace customtabs {

static jboolean JNI_CustomTabsTestUtils_HasVariationId(JNIEnv* env, jint id) {
  auto ids = variations::VariationsIdsProvider::GetInstance()
                 ->GetVariationsVectorForWebPropertiesKeys();
  return base::Contains(ids, id);
}

}  // namespace customtabs
