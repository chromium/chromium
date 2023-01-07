// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/jni_translation.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "components/feed/core/proto/v2/ui.pb.h"

namespace feed {
namespace android {

LoggingParameters ToNativeLoggingParameters(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& logging_parameters) {
  std::string bytes;
  base::android::JavaByteArrayToString(env, logging_parameters, &bytes);
  feedui::LoggingParameters logging_parameters_value;
  if (!logging_parameters_value.ParseFromString(bytes)) {
    DLOG(ERROR) << "Error parsing logging parameters";
    return {};
  }

  return FromProto(logging_parameters_value);
}

}  // namespace android
}  // namespace feed
