// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/files/file_path.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/internal/android/jni_headers/RenameUtils_jni.h"

// static
static std::string JNI_RenameUtils_GetFileExtension(
    const std::string& file_name) {
  return base::FilePath(file_name).Extension();
}

DEFINE_JNI(RenameUtils)
