// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"

#include "chrome/browser/page_content_annotations/android/page_content_extraction_service_android.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/page_content_annotations/factory_jni_headers/PageContentExtractionServiceFactory_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace page_content_annotations {

static ScopedJavaLocalRef<jobject>
JNI_PageContentExtractionServiceFactory_GetForProfile(JNIEnv* env,
                                                      Profile* profile) {
  DCHECK(profile);
  PageContentExtractionService* service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  return PageContentExtractionService::GetJavaObject(service);
}

}  // namespace page_content_annotations

DEFINE_JNI(PageContentExtractionServiceFactory)
