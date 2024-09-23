// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_
#define CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_

#include <jni.h>
#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"

class Profile;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace chrome {
namespace android {

// JNI bridge for   RlzPingHandler.java
class RlzPingHandler {
 public:
  explicit RlzPingHandler(Profile* profile);

  RlzPingHandler(const RlzPingHandler&) = delete;
  RlzPingHandler& operator=(const RlzPingHandler&) = delete;

  ~RlzPingHandler();

  // Makes a GET request to the designated web end point with the given
  // parameters. |j_brand| is a 4 character priorly designated brand value.
  // |j_language| is the 2 letter lower case language. |events| is a single
  // string where multiple 4 character long events are concatenated with ,
  // and |id| is a unique id for the device that is 50 characters long.
  void Ping(const base::android::JavaParamRef<jstring>& j_brand,
            const base::android::JavaParamRef<jstring>& j_language,
            const base::android::JavaParamRef<jstring>& j_events,
            const base::android::JavaParamRef<jstring>& j_id,
            const base::android::JavaParamRef<jobject>& j_callback);

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::android::ScopedJavaGlobalRef<jobject> j_callback_;
};

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_RLZ_RLZ_PING_HANDLER_H_
