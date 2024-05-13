// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_resumption/visited_url_ranking_backend.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/tab_resumption/jni_headers/VisitedUrlRankingBackend_jni.h"

namespace tab_resumption::jni {

static jlong JNI_VisitedUrlRankingBackend_Init(JNIEnv* env, Profile* profile) {
  return reinterpret_cast<intptr_t>(new VisitedUrlRankingBackend(profile));
}

VisitedUrlRankingBackend::VisitedUrlRankingBackend(Profile* profile)
    : profile_(profile) {}

VisitedUrlRankingBackend::~VisitedUrlRankingBackend() = default;

void VisitedUrlRankingBackend::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace tab_resumption::jni
