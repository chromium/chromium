// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/library_loader/library_loader_hooks.h"

#include "base/android/jni_string.h"
#include "base/android/library_loader/anchor_functions_buildflags.h"
#include "base/android/library_loader/library_load_from_apk_status_codes.h"
#include "base/android/library_loader/library_prefetcher.h"
#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "jni/LibraryLoader_jni.h"

#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
#include "base/android/orderfile/orderfile_instrumentation.h"
#endif

namespace base {
namespace android {

namespace {

base::AtExitManager* g_at_exit_manager = NULL;
const char* g_library_version_number = "";
LibraryLoadedHook* g_registration_callback = NULL;
NativeInitializationHook* g_native_initialization_hook = NULL;

enum RendererHistogramCode {
  // Renderer load at fixed address success, fail, or not attempted.
  // Renderers do not attempt to load at at fixed address if on a
  // low-memory device on which browser load at fixed address has already
  // failed.
  LFA_SUCCESS = 0,
  LFA_BACKOFF_USED = 1,
  LFA_NOT_ATTEMPTED = 2,

  // End sentinel, also used as nothing-pending indicator.
  MAX_RENDERER_HISTOGRAM_CODE = 3,
  NO_PENDING_HISTOGRAM_CODE = MAX_RENDERER_HISTOGRAM_CODE
};

enum BrowserHistogramCode {
  // Non-low-memory random address browser loads.
  NORMAL_LRA_SUCCESS = 0,

  // Low-memory browser loads at fixed address, success or fail.
  LOW_MEMORY_LFA_SUCCESS = 1,
  LOW_MEMORY_LFA_BACKOFF_USED = 2,

  MAX_BROWSER_HISTOGRAM_CODE = 3,
};

RendererHistogramCode g_renderer_histogram_code = NO_PENDING_HISTOGRAM_CODE;

// Indicate whether g_library_preloader_renderer_histogram_code is valid
bool g_library_preloader_renderer_histogram_code_registered = false;

// The return value of NativeLibraryPreloader.loadLibrary() in child processes,
// it is initialized to the invalid value which shouldn't showup in UMA report.
int g_library_preloader_renderer_histogram_code = -1;

// The amount of time, in milliseconds, that it took to load the shared
// libraries in the renderer. Set in
// RegisterChromiumAndroidLinkerRendererHistogram.
long g_renderer_library_load_time_ms = 0;

void RecordChromiumAndroidLinkerRendererHistogram() {
  if (g_renderer_histogram_code == NO_PENDING_HISTOGRAM_CODE)
    return;
  // Record and release the pending histogram value.
  UMA_HISTOGRAM_ENUMERATION("ChromiumAndroidLinker.RendererStates",
                            g_renderer_histogram_code,
                            MAX_RENDERER_HISTOGRAM_CODE);
  g_renderer_histogram_code = NO_PENDING_HISTOGRAM_CODE;

  // Record how long it took to load the shared libraries.
  UMA_HISTOGRAM_TIMES("ChromiumAndroidLinker.RendererLoadTime",
      base::TimeDelta::FromMilliseconds(g_renderer_library_load_time_ms));
}

void RecordLibraryPreloaderRendereHistogram() {
  if (g_library_preloader_renderer_histogram_code_registered) {
    UmaHistogramSparse("Android.NativeLibraryPreloader.Result.Renderer",
                       g_library_preloader_renderer_histogram_code);
  }
}

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
bool ShouldDoOrderfileMemoryOptimization() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kOrderfileMemoryOptimization);
}
#endif

}  // namespace

static void JNI_LibraryLoader_RegisterChromiumAndroidLinkerRendererHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean requested_shared_relro,
    jboolean load_at_fixed_address_failed,
    jlong library_load_time_ms) {
  // Note a pending histogram value for later recording.
  if (requested_shared_relro) {
    g_renderer_histogram_code = load_at_fixed_address_failed
                                ? LFA_BACKOFF_USED : LFA_SUCCESS;
  } else {
    g_renderer_histogram_code = LFA_NOT_ATTEMPTED;
  }

  g_renderer_library_load_time_ms = library_load_time_ms;
}

static void JNI_LibraryLoader_RecordChromiumAndroidLinkerBrowserHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean is_using_browser_shared_relros,
    jboolean load_at_fixed_address_failed,
    jint library_load_from_apk_status,
    jlong library_load_time_ms) {
  // For low-memory devices, record whether or not we successfully loaded the
  // browser at a fixed address. Otherwise just record a normal invocation.
  BrowserHistogramCode histogram_code;
  if (is_using_browser_shared_relros) {
    histogram_code = load_at_fixed_address_failed
                     ? LOW_MEMORY_LFA_BACKOFF_USED : LOW_MEMORY_LFA_SUCCESS;
  } else {
    histogram_code = NORMAL_LRA_SUCCESS;
  }
  UMA_HISTOGRAM_ENUMERATION("ChromiumAndroidLinker.BrowserStates",
                            histogram_code,
                            MAX_BROWSER_HISTOGRAM_CODE);

  // Record the device support for loading a library directly from the APK file.
  UMA_HISTOGRAM_ENUMERATION(
      "ChromiumAndroidLinker.LibraryLoadFromApkStatus",
      static_cast<LibraryLoadFromApkStatusCodes>(library_load_from_apk_status),
      LIBRARY_LOAD_FROM_APK_STATUS_CODES_MAX);

  // Record how long it took to load the shared libraries.
  UMA_HISTOGRAM_TIMES("ChromiumAndroidLinker.BrowserLoadTime",
                      base::TimeDelta::FromMilliseconds(library_load_time_ms));
}

static void JNI_LibraryLoader_RecordLibraryPreloaderBrowserHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint status) {
  UmaHistogramSparse("Android.NativeLibraryPreloader.Result.Browser", status);
}

static void JNI_LibraryLoader_RegisterLibraryPreloaderRendererHistogram(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint status) {
  g_library_preloader_renderer_histogram_code = status;
  g_library_preloader_renderer_histogram_code_registered = true;
}

void SetNativeInitializationHook(
    NativeInitializationHook native_initialization_hook) {
  g_native_initialization_hook = native_initialization_hook;
}

void RecordLibraryLoaderRendererHistograms() {
  RecordChromiumAndroidLinkerRendererHistogram();
  RecordLibraryPreloaderRendereHistogram();
}

void SetLibraryLoadedHook(LibraryLoadedHook* func) {
  g_registration_callback = func;
}

static jboolean JNI_LibraryLoader_LibraryLoaded(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jint library_process_type) {
#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  orderfile::StartDelayedDump();
#endif

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  if (ShouldDoOrderfileMemoryOptimization()) {
    NativeLibraryPrefetcher::MadviseForOrderfile();
  }
#endif

  if (g_native_initialization_hook &&
      !g_native_initialization_hook(
          static_cast<LibraryProcessType>(library_process_type)))
    return false;
  if (g_registration_callback &&
      !g_registration_callback(
          env, nullptr,
          static_cast<LibraryProcessType>(library_process_type))) {
    return false;
  }
  return true;
}

void LibraryLoaderExitHook() {
  if (g_at_exit_manager) {
    delete g_at_exit_manager;
    g_at_exit_manager = NULL;
  }
}

static void JNI_LibraryLoader_ForkAndPrefetchNativeLibrary(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  return NativeLibraryPrefetcher::ForkAndPrefetchNativeLibrary(
      ShouldDoOrderfileMemoryOptimization());
#endif
}

static jint JNI_LibraryLoader_PercentageOfResidentNativeLibraryCode(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  return NativeLibraryPrefetcher::PercentageOfResidentNativeLibraryCode();
#else
  return -1;
#endif
}

static void JNI_LibraryLoader_PeriodicallyCollectResidency(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
#if BUILDFLAG(SUPPORTS_CODE_ORDERING)
  NativeLibraryPrefetcher::PeriodicallyCollectResidency();
#else
  LOG(WARNING) << "Collecting residency is not supported.";
#endif
}

void SetVersionNumber(const char* version_number) {
  g_library_version_number = strdup(version_number);
}

ScopedJavaLocalRef<jstring> JNI_LibraryLoader_GetVersionNumber(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return ConvertUTF8ToJavaString(env, g_library_version_number);
}

void InitAtExitManager() {
  g_at_exit_manager = new base::AtExitManager();
}

}  // namespace android
}  // namespace base
