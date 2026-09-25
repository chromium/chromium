// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_ANDROID_H_
#define BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_ANDROID_H_

#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/traits.h"

namespace base::android {

// Flushes any MemoryConsumer registrations that were queued in Java before
// native initialization.
BASE_EXPORT void FlushPendingMemoryConsumerRegistrations();

// Notifies Java that the native MemoryConsumerRegistry has been destroyed.
BASE_EXPORT void OnMemoryConsumerRegistryDestroyed();

// C++ native peer for Java MemoryConsumer implementations.
class BASE_EXPORT MemoryConsumerAndroid : public base::MemoryConsumer {
 public:
  MemoryConsumerAndroid(JNIEnv* env,
                        const base::android::JavaRef<jobject>& java_consumer,
                        std::string_view consumer_name,
                        base::MemoryConsumerTraits traits);
  ~MemoryConsumerAndroid() override;

  MemoryConsumerAndroid(const MemoryConsumerAndroid&) = delete;
  MemoryConsumerAndroid& operator=(const MemoryConsumerAndroid&) = delete;

  void Destroy();
  void NotifyInitialLimitIfNonDefault();

  // base::MemoryConsumer:
  void OnUpdateMemoryLimit() override;
  void OnReleaseMemory() override;
  bool IsPassive() const override;

 private:
  const bool is_passive_;
  base::android::ScopedJavaGlobalRef<jobject> java_consumer_;
  base::MemoryConsumerRegistration registration_;
};

}  // namespace base::android

#endif  // BASE_MEMORY_COORDINATOR_MEMORY_CONSUMER_ANDROID_H_
