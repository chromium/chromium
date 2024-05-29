// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/resource_mapper.h"

#include <map>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/grit/components_scaled_resources.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ResourceMapper_jni.h"

namespace {

typedef std::map<int, int> ResourceMap;
base::LazyInstance<ResourceMap>::Leaky g_id_map = LAZY_INSTANCE_INITIALIZER;

} // namespace

const int ResourceMapper::kMissingId = 0;

int ResourceMapper::MapToJavaDrawableId(int resource_id) {
  if (g_id_map.Get().empty()) {
    ConstructMap();
  }

  ResourceMap::iterator iterator = g_id_map.Get().find(resource_id);
  if (iterator != g_id_map.Get().end()) {
    return iterator->second;
  }

  // The resource couldn't be found.
  NOTREACHED_IN_MIGRATION();
  return kMissingId;
}

void ResourceMapper::ConstructMap() {
  DCHECK(g_id_map.Get().empty());
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jintArray> java_id_array =
      Java_ResourceMapper_getResourceIdList(env);
  std::vector<int> resource_id_list;
  base::android::JavaIntArrayToIntVector(env, java_id_array, &resource_id_list);
  size_t next_id = 0;

#define LINK_RESOURCE_ID(c_id, java_id) \
  g_id_map.Get()[c_id] = resource_id_list[next_id++];
#define DECLARE_RESOURCE_ID(c_id, java_id) \
  g_id_map.Get()[c_id] = resource_id_list[next_id++];
#include "chrome/browser/android/resource_id.h"
#include "components/resources/android/autofill_resource_id.h"
#include "components/resources/android/blocked_content_resource_id.h"
#include "components/resources/android/page_info_resource_id.h"
#include "components/resources/android/permissions_resource_id.h"
#include "components/resources/android/sms_resource_id.h"
#include "components/resources/android/webxr_resource_id.h"
#undef LINK_RESOURCE_ID
#undef DECLARE_RESOURCE_ID
  // Make sure ID list sizes match up.
  DCHECK_EQ(next_id, resource_id_list.size());
}
