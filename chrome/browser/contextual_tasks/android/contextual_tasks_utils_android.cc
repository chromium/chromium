// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/public/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/contextual_tasks/utils_jni_headers/ContextualTasksUtils_jni.h"

static bool JNI_ContextualTasksUtils_IsContextualTasksUiEnabled() {
  return contextual_tasks::IsContextualTasksUIEnabled();
}

DEFINE_JNI(ContextualTasksUtils)
