// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_callback_validator.h"

DownloadCallbackValidator::DownloadCallbackValidator() = default;

DownloadCallbackValidator::~DownloadCallbackValidator() = default;

void DownloadCallbackValidator::AddJavaCallback(intptr_t callback_id) {
  callback_ids_.emplace(callback_id);
}

bool DownloadCallbackValidator::ValidateAndClearJavaCallback(
    intptr_t callback_id) {
  if (callback_ids_.find(callback_id) == callback_ids_.end()) {
    return false;
  }
  callback_ids_.erase(callback_id);
  return true;
}
