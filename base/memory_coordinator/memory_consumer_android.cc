// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_coordinator/memory_consumer_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/memory_coordinator/memory_limit.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/memory_coordinator_jni/MemoryConsumerRegistration_jni.h"
#include "base/memory_coordinator_jni/MemoryConsumerTraits_jni.h"

namespace base::android {

namespace {

// LINT.IfChange(TraitsFromJava)
base::MemoryConsumerTraits TraitsFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_traits) {
  const int packed = Java_MemoryConsumerTraits_getPackedTraits(env, j_traits);

  const auto consumer_type =
      static_cast<base::MemoryConsumerTraits::ConsumerType>(packed & 0x1);
  const auto estimated_memory_usage =
      static_cast<base::MemoryConsumerTraits::EstimatedMemoryUsage>(
          (packed >> 1) & 0x3);
  const auto release_memory_cost =
      static_cast<base::MemoryConsumerTraits::ReleaseMemoryCost>((packed >> 3) &
                                                                 0x3);
  const auto information_retention =
      static_cast<base::MemoryConsumerTraits::InformationRetention>(
          (packed >> 5) & 0x3);
  const auto execution_type =
      static_cast<base::MemoryConsumerTraits::ExecutionType>((packed >> 7) &
                                                             0x1);
  const auto supports_memory_limit =
      static_cast<base::MemoryConsumerTraits::SupportsMemoryLimit>(
          (packed >> 8) & 0x1);
  const auto in_process =
      static_cast<base::MemoryConsumerTraits::InProcess>((packed >> 9) & 0x3);
  const auto recreate_memory_cost =
      static_cast<base::MemoryConsumerTraits::RecreateMemoryCost>(
          (packed >> 11) & 0x3);
  const auto release_gc_references =
      static_cast<base::MemoryConsumerTraits::ReleaseGCReferences>(
          (packed >> 13) & 0x1);
  const auto garbage_collects_v8_heap =
      static_cast<base::MemoryConsumerTraits::GarbageCollectsV8Heap>(
          (packed >> 14) & 0x1);
  const auto is_stateful =
      static_cast<base::MemoryConsumerTraits::IsStateful>((packed >> 15) & 0x1);

  if (consumer_type == base::MemoryConsumerTraits::ConsumerType::kPassive) {
    return base::MemoryConsumerTraits(consumer_type, supports_memory_limit,
                                      in_process, release_gc_references);
  }

  return base::MemoryConsumerTraits(
      estimated_memory_usage, release_memory_cost, information_retention,
      execution_type, supports_memory_limit, in_process, recreate_memory_cost,
      release_gc_references, garbage_collects_v8_heap, is_stateful);
}
// LINT.ThenChange(//base/android/java/src/org/chromium/base/memory_coordinator/MemoryConsumerTraits.java)

bool SupportsJavaMemoryConsumers() {
  auto process_type = base::android::GetLibraryProcessType();
  return process_type == base::android::PROCESS_BROWSER ||
         process_type == base::android::PROCESS_WEBVIEW;
}

}  // namespace

MemoryConsumerAndroid::MemoryConsumerAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_consumer,
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits)
    : is_passive_(traits.consumer_type ==
                  base::MemoryConsumerTraits::ConsumerType::kPassive),
      java_consumer_(env, java_consumer),
      registration_(
          consumer_name,
          traits,
          this,
          base::MemoryConsumerRegistration::CheckUnregister::kDisabled) {}

MemoryConsumerAndroid::~MemoryConsumerAndroid() = default;

void MemoryConsumerAndroid::Destroy() {
  delete this;
}

void MemoryConsumerAndroid::NotifyInitialLimitIfNonDefault() {
  if (memory_limit() != base::MemoryLimit::Default()) {
    OnUpdateMemoryLimit();
  }
}

void MemoryConsumerAndroid::OnUpdateMemoryLimit() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MemoryConsumerRegistration_onUpdateMemoryLimit(env, java_consumer_,
                                                      memory_limit().percent());
}

void MemoryConsumerAndroid::OnReleaseMemory() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MemoryConsumerRegistration_onReleaseMemory(env, java_consumer_);
}

bool MemoryConsumerAndroid::IsPassive() const {
  return is_passive_;
}

static int64_t JNI_MemoryConsumerRegistration_Register(
    JNIEnv* env,
    const jni_zero::JavaRef<jstring>& j_consumer_name,
    const jni_zero::JavaRef<jobject>& j_traits,
    const jni_zero::JavaRef<jobject>& j_consumer) {
  std::string consumer_name =
      base::android::ConvertJavaStringToUTF8(env, j_consumer_name);
  base::MemoryConsumerTraits traits =
      base::android::TraitsFromJava(env, j_traits);
  auto* native_peer = new base::android::MemoryConsumerAndroid(
      env, j_consumer, consumer_name, traits);
  return reinterpret_cast<intptr_t>(native_peer);
}

void FlushPendingMemoryConsumerRegistrations() {
  if (!SupportsJavaMemoryConsumers()) {
    return;
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MemoryConsumerRegistration_flushPendingRegistrations(env);
}

void OnMemoryConsumerRegistryDestroyed() {
  if (!SupportsJavaMemoryConsumers()) {
    return;
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_MemoryConsumerRegistration_onNativeRegistryDestroyed(env);
}

}  // namespace base::android

DEFINE_JNI(MemoryConsumerRegistration)
DEFINE_JNI(MemoryConsumerTraits)
