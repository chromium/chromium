// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/visited_url_ranking/group_suggestions_service_factory.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/tab_group_suggestion/jni_headers/GroupSuggestionsServiceFactory_jni.h"

static base::android::ScopedJavaLocalRef<jobject>
JNI_GroupSuggestionsServiceFactory_GetForProfile(JNIEnv* env,
                                                 Profile* profile) {
  DCHECK(profile);
  visited_url_ranking::GroupSuggestionsService* service =
      visited_url_ranking::GroupSuggestionsServiceFactory::GetForProfile(
          profile);
  return visited_url_ranking::GroupSuggestionsService::GetJavaObject(service);
}
