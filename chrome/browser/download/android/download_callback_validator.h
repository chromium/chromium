// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CALLBACK_VALIDATOR_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CALLBACK_VALIDATOR_H_

#include <set>

// Helper class used to validate callbacks that passed to Java
// side are used correctly.
class DownloadCallbackValidator {
 public:
  DownloadCallbackValidator();
  ~DownloadCallbackValidator();

  DownloadCallbackValidator(const DownloadCallbackValidator&) = delete;
  DownloadCallbackValidator& operator=(const DownloadCallbackValidator&) =
      delete;

  // Adds a java callback id that will be called later.
  void AddJavaCallback(intptr_t callback_id);

  // Validate the java callback id is valid, and remove it from
  // |callback_ids|.
  bool ValidateAndClearJavaCallback(intptr_t callback_id);

 private:
  // Callback IDs, used for validation purpose.
  std::set<intptr_t> callback_ids_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CALLBACK_VALIDATOR_H_
