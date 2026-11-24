// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <iterator>
#include <optional>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_event.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/tasks_jni/ThreadUtils_jni.h"

namespace base {

// Allows fine-grained control of thread priorities on Android.
// Enable with e.g.
// --enable-features=AndroidThreadPriority:presentation/-8/default/-1
BASE_FEATURE(kAndroidThreadPriority, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr FeatureParam<int> kBackgroundThreadPriority{&kAndroidThreadPriority,
                                                      "background", 10};
constexpr FeatureParam<int> kUtilityThreadPriority{&kAndroidThreadPriority,
                                                   "utility", 1};
constexpr FeatureParam<int> kDefaultThreadPriority{&kAndroidThreadPriority,
                                                   "default", 0};
constexpr FeatureParam<int> kPresentationThreadPriority{&kAndroidThreadPriority,
                                                        "presentation", -4};
constexpr FeatureParam<int> kInteractiveThreadPriority{&kAndroidThreadPriority,
                                                       "interactive", -4};
constexpr FeatureParam<bool> kObeySocRestrictions{
    &kAndroidThreadPriority, "obey_soc_restrictions", true};
// kRealtimeAudio cannot be configured on the command line and should be the
// effective maximum.

// When enabled, do not run threads with a less important ThreadType than
// kDisplayCritical on the big core cluster, for configurations with at least 3
// clusters. This is based on observations that this cluster is both
// power-hungry and contended.
BASE_FEATURE(kRestrictBigCoreThreadAffinity, base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr int kRealtimeAudioNice = -16;
// ADPF-equipped Google Pixels can suffer input jank if the presentation nice
// value is lowered below -4.
constexpr int kGoogleSocMinPresentationNice = -4;

// Use atomics to access FeatureList values when setting up a thread, since
// there are cases when FeatureList initialization is not synchronized with
// PlatformThread creation (including ScopedFeatureList in tests).
constinit std::atomic<int> g_background_nice{
    kBackgroundThreadPriority.default_value};
constinit std::atomic<int> g_utility_nice{kUtilityThreadPriority.default_value};
constinit std::atomic<int> g_default_nice{kDefaultThreadPriority.default_value};
constinit std::atomic<int> g_presentation_nice{
    kPresentationThreadPriority.default_value};
constinit std::atomic<int> g_audio_processing_nice{
    kInteractiveThreadPriority.default_value};
constinit std::atomic<bool> g_restrict_big_core_thread_affinity{false};

std::vector<uint64_t>* g_max_frequency_per_processor_override = nullptr;

struct SetAffinityMask {
  // Only set when the current CPU configuration has at least 3 separate
  // clusters.
  std::optional<cpu_set_t> mask;
  size_t count = 0;
  int allowed_count = 0;
};

// Get the CPU affinity mask, given the maximum frequency of all cores, and
// whether the mask should allow to run on the largest core cluster, for
// configurations with at least 3 clusters.
//
// Returns a value where `mask.mask` is not set if the device is not eligible,
// that is it does not have at least 3 different CPU types.
SetAffinityMask GetAffinityMask(const std::vector<uint64_t>& max_frequencies,
                                bool can_run) {
  if (max_frequencies.empty()) {
    return {};
  }

  auto sorted = max_frequencies;
  std::sort(sorted.begin(), sorted.end());
  uint64_t max_frequency = sorted[sorted.size() - 1];
  auto last = std::unique(sorted.begin(), sorted.end());
  ssize_t distinct_count = std::distance(sorted.begin(), last);

  // Don't want to move entirely from big cores on big.LITTLE, only on
  // little-mid-big designs.
  if (distinct_count < 3) {
    return {};
  }

  int allowed_cpus_count = 0;
  cpu_set_t cpu_set;
  // SAFETY: Here and below, these are macros that we don't control, and hence
  // we cannot safely replace. However, CPU_ZERO() is safe, and CPU_SET() has a
  // check internally to not overflow the bitset, which we repeat in the loop to
  // be clearer.
  UNSAFE_BUFFERS(CPU_ZERO(&cpu_set));
  for (size_t i = 0; i < max_frequencies.size(); i++) {
    if (i < CPU_SETSIZE) {
      if (can_run || (max_frequencies[i] < max_frequency)) {
        allowed_cpus_count++;
        UNSAFE_BUFFERS(CPU_SET(i, &cpu_set));
      }
    }
  }

  return {cpu_set, max_frequencies.size(), allowed_cpus_count};
}

}  // namespace

void SetMaxFrequencyPerProcessorOverrideForTesting(
    std::vector<uint64_t>* value) {
  g_max_frequency_per_processor_override = value;
}

bool IsEligibleForBigCoreAffinityChange() {
  if (g_max_frequency_per_processor_override) {
    return GetAffinityMask(*g_max_frequency_per_processor_override, false)
        .mask.has_value();
  }
  static const bool eligible =
      GetAffinityMask(SysInfo::MaxFrequencyPerProcessor(), false)
          .mask.has_value();
  return eligible;
}

void SetCanRunOnBigCore(PlatformThreadId thread_id, bool can_run) {
  TRACE_EVENT("base", __PRETTY_FUNCTION__, "thread_id", thread_id, "can_run",
              can_run);
  SetAffinityMask mask;
  if (g_max_frequency_per_processor_override) {
    mask = GetAffinityMask(*g_max_frequency_per_processor_override, can_run);
  } else {
    static const SetAffinityMask all_cores_mask =
        GetAffinityMask(SysInfo::MaxFrequencyPerProcessor(), true);
    static const SetAffinityMask no_big_cores_mask =
        GetAffinityMask(SysInfo::MaxFrequencyPerProcessor(), false);
    mask = can_run ? all_cores_mask : no_big_cores_mask;
  }

  if (!mask.mask) {
    return;
  }

  TRACE_EVENT("base", "SetAffinity", "count", mask.count, "allowed",
              mask.allowed_count);
  // If the call fails, it's not a correctness issue. However we want to catch
  // the sandbox returning EPERM.
  //
  // For instance, an invalid mask (e.g. one with an empty intersection with the
  // set of possible CPUs) returns EINVAL and does not change the current mask,
  // per sched_setaffinity(2). On more recent kernels, an empty mask resets the
  // affinity.
  int retval =
      sched_setaffinity(thread_id.raw(), sizeof(*mask.mask), &*mask.mask);
  DPCHECK(!retval);
}

namespace internal {

// - kRealtimeAudio corresponds to Android's PRIORITY_AUDIO = -16 value.
// - kPresentation corresponds to Android's PRIORITY_DISPLAY = -4 value.
// - kUtility corresponds to Android's THREAD_PRIORITY_LESS_FAVORABLE = 1 value.
// - kBackground corresponds to Android's PRIORITY_BACKGROUND = 10
//   value. Contrary to the matching Java APi in Android <13, this does not
//   restrict the thread to (subset of) little cores.
const ThreadTypeToNiceValuePairForTest kThreadTypeToNiceValueMapForTest[7] = {
    {ThreadType::kRealtimeAudio, -16}, {ThreadType::kPresentation, -4},
    {ThreadType::kDefault, 0},         {ThreadType::kUtility, 1},
    {ThreadType::kBackground, 10},
};

// Returns the cached nice value for `thread_type`. Before InitializeFeatures()
// runs or when kAndroidThreadPriority is disabled, the defaults are:
//    - kBackground: Android's PRIORITY_BACKGROUND = 10.
//    - kUtility: Android's THREAD_PRIORITY_LESS_FAVORABLE = 1.
//    - kDefault: 0.
//    - kPresentation and kAudioProcessing: Android's PRIORITY_DISPLAY = -4.
//    - kRealtimeAudio: Android's PRIORITY_AUDIO = -16.
int ThreadTypeToNiceValue(const ThreadType thread_type) {
  switch (thread_type) {
    case ThreadType::kBackground:
      return g_background_nice.load(std::memory_order_relaxed);
    case ThreadType::kUtility:
      return g_utility_nice.load(std::memory_order_relaxed);
    case ThreadType::kDefault:
      return g_default_nice.load(std::memory_order_relaxed);
    case ThreadType::kPresentation:
      return g_presentation_nice.load(std::memory_order_relaxed);
    case ThreadType::kAudioProcessing:
      return g_audio_processing_nice.load(std::memory_order_relaxed);
    case ThreadType::kRealtimeAudio:
      return kRealtimeAudioNice;
  }
}

bool CanSetThreadTypeToRealtimeAudio() {
  return true;
}

void SetCurrentThreadTypeImpl(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
  // We set the Audio priority through JNI as the Java setThreadPriority will
  // put it into a preferable cgroup, whereas the "normal" C++ call wouldn't.
  // However, with
  // https://android-review.googlesource.com/c/platform/system/core/+/1975808
  // this becomes obsolete and we can avoid this starting in API level 33.
  if (thread_type == ThreadType::kRealtimeAudio &&
      base::android::android_info::sdk_int() <
          base::android::android_info::SDK_VERSION_T) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_ThreadUtils_setThreadPriorityAudio(env,
                                            PlatformThread::CurrentId().raw());
  } else if (thread_type == ThreadType::kPresentation &&
             pump_type_hint == MessagePumpType::UI &&
             GetCurrentThreadNiceValue() <=
                 ThreadTypeToNiceValue(ThreadType::kPresentation)) {
    // Recent versions of Android (O+) up the priority of the UI thread
    // automatically.
  } else {
    SetThreadNiceFromType(PlatformThread::CurrentId(), thread_type);
  }

  if (g_restrict_big_core_thread_affinity.load(std::memory_order_relaxed)) {
    SetCanRunOnBigCore(PlatformThread::CurrentId(),
                       thread_type >= ThreadType::kPresentation);
  }
}

std::optional<ThreadType> GetCurrentEffectiveThreadTypeForPlatformForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (Java_ThreadUtils_isThreadPriorityAudio(
          env, PlatformThread::CurrentId().raw())) {
    return std::make_optional(ThreadType::kRealtimeAudio);
  }
  return std::nullopt;
}

PlatformPriorityOverride SetThreadTypeOverride(
    PlatformThreadHandle thread_handle,
    ThreadType thread_type) {
  PlatformThreadId thread_id(
      pthread_gettid_np(thread_handle.platform_handle()));
  if (GetThreadNiceValue(thread_id) <= ThreadTypeToNiceValue(thread_type)) {
    return false;
  }
  return SetThreadNiceFromType(thread_id, thread_type);
}

void RemoveThreadTypeOverride(
    PlatformThreadHandle thread_handle,
    const PlatformPriorityOverride& priority_override_handle,
    ThreadType initial_thread_type) {
  if (!priority_override_handle) {
    return;
  }

  PlatformThreadId thread_id(
      pthread_gettid_np(thread_handle.platform_handle()));
  SetThreadNiceFromType(thread_id, initial_thread_type);
}

}  // namespace internal

void PlatformThreadBase::SetName(const std::string& name) {
  SetNameCommon(name);

  // Like linux, on android we can get the thread names to show up in the
  // debugger by setting the process name for the LWP.
  // We don't want to do this for the main thread because that would rename
  // the process, causing tools like killall to stop working.
  if (PlatformThread::CurrentId().raw() == getpid()) {
    return;
  }

  // Set the name for the LWP (which gets truncated to 15 characters).
  int err = prctl(PR_SET_NAME, name.c_str());
  if (err < 0 && errno != EPERM) {
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
  }
}

// static
void PlatformThreadAndroid::InitializeFeatures() {
  int background = kBackgroundThreadPriority.default_value;
  int utility = kUtilityThreadPriority.default_value;
  int default_prio = kDefaultThreadPriority.default_value;
  int presentation = kPresentationThreadPriority.default_value;
  int audio_processing = kInteractiveThreadPriority.default_value;
  if (FeatureList::IsEnabled(kAndroidThreadPriority)) {
    background = kBackgroundThreadPriority.Get();
    utility = kUtilityThreadPriority.Get();
    default_prio = kDefaultThreadPriority.Get();
    static const bool is_google_soc = SysInfo::SocManufacturer() == "Google";
    presentation = kPresentationThreadPriority.Get();
    if (is_google_soc && kObeySocRestrictions.Get()) {
      presentation = std::max(kGoogleSocMinPresentationNice, presentation);
    }
    audio_processing = kInteractiveThreadPriority.Get();

    // Establish weak partial ordering from high to low.
    DCHECK_LE(utility, background);
    DCHECK_LE(default_prio, utility);
    DCHECK_LE(presentation, default_prio);
    DCHECK_LE(audio_processing, presentation);
    // Check that -16 is the highest priority in the system.
    DCHECK_LT(kRealtimeAudioNice, audio_processing);
  }
  g_background_nice.store(background, std::memory_order_relaxed);
  g_utility_nice.store(utility, std::memory_order_relaxed);
  g_default_nice.store(default_prio, std::memory_order_relaxed);
  g_presentation_nice.store(presentation, std::memory_order_relaxed);
  g_audio_processing_nice.store(audio_processing, std::memory_order_relaxed);

  // Only query `kRestrictBigCoreThreadAffinity` on eligible devices so
  // ineligible devices are not included in the experiment.
  g_restrict_big_core_thread_affinity.store(
      IsEligibleForBigCoreAffinityChange() &&
          FeatureList::IsEnabled(kRestrictBigCoreThreadAffinity),
      std::memory_order_relaxed);
}

void InitThreading() {}

void TerminateOnThread() {
  base::android::DetachFromVM();
}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
#if !defined(ADDRESS_SANITIZER)
  return 0;
#else
  // AddressSanitizer bloats the stack approximately 2x. Default stack size of
  // 1Mb is not enough for some tests (see http://crbug.com/263749 for example).
  return 2 * (1 << 20);  // 2Mb
#endif
}

}  // namespace base

DEFINE_JNI(ThreadUtils)
