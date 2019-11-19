// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NET_NQE_NETWORK_QUALITY_PROVIDER_H_
#define CHROME_BROWSER_ANDROID_NET_NQE_NETWORK_QUALITY_PROVIDER_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"
#include "services/network/public/cpp/network_quality_tracker.h"

// The native instance of the NetworkQualityProvider. This class is not
// threadsafe and must only be used on the UI thread.
class NetworkQualityProvider
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver {
 public:
  NetworkQualityProvider(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

 private:
  // Note that this destructor is currently dead code. This destructor is never
  // called as this object is owned by a java singleton that never goes away.
  ~NetworkQualityProvider() override;

  // net::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // net::RTTAndThroughputEstimatesObserver implementation:
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override;

  base::android::ScopedJavaGlobalRef<jobject> j_obj_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkQualityProvider);
};

#endif  // CHROME_BROWSER_ANDROID_NET_NQE_NETWORK_QUALITY_PROVIDER_H_
