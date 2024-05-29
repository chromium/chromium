// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/warm_compositor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/paint_preview/android/jni_headers/PaintPreviewCompositorUtils_jni.h"

namespace paint_preview {

void JNI_PaintPreviewCompositorUtils_WarmupCompositor(JNIEnv* env) {
  WarmCompositor::GetInstance()->WarmupCompositor();
}

jboolean JNI_PaintPreviewCompositorUtils_StopWarmCompositor(JNIEnv* env) {
  return static_cast<jboolean>(WarmCompositor::GetInstance()->StopCompositor());
}

}  // namespace paint_preview
