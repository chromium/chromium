// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INTENT_HELPER_H_
#define CHROME_BROWSER_ANDROID_INTENT_HELPER_H_

#include <jni.h>

#include <string>


namespace chrome {
namespace android {

// Triggers a send email intent.
void SendEmail(const std::u16string& data_email,
               const std::u16string& data_subject,
               const std::u16string& data_body,
               const std::u16string& data_chooser_title,
               const std::u16string& data_file_to_attach);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_INTENT_HELPER_H_
