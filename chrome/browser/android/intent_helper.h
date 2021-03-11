// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_INTENT_HELPER_H_
#define CHROME_BROWSER_ANDROID_INTENT_HELPER_H_

#include <jni.h>

#include <string>

#include "base/strings/string16.h"

namespace chrome {
namespace android {

// Triggers a send email intent.
void SendEmail(const base::string16& data_email,
               const base::string16& data_subject,
               const base::string16& data_body,
               const base::string16& data_chooser_title,
               const base::string16& data_file_to_attach);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_INTENT_HELPER_H_
