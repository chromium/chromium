// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/crash_reporter/aw_crash_reporter_client.h"

#include <stdint.h>

#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "base/android/build_info.h"
#include "base/android/java_exception_reporter.h"
#include "base/android/jni_android.h"
#include "base/base_paths_android.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "components/crash/core/app/crash_reporter_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/version_info/android/channel_getter.h"
#include "components/version_info/version_info.h"
#include "components/version_info/version_info_values.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/crash_client_jni/AwCrashReporterClient_jni.h"

using base::android::AttachCurrentThread;

namespace android_webview {

namespace {

class AwCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  AwCrashReporterClient() = default;

  AwCrashReporterClient(const AwCrashReporterClient&) = delete;
  AwCrashReporterClient& operator=(const AwCrashReporterClient&) = delete;

  // crash_reporter::CrashReporterClient implementation.
  bool IsRunningUnattended() override { return false; }
  bool GetCollectStatsConsent() override {
    // TODO(jperaza): Crashpad uses GetCollectStatsConsent() to enable or
    // disable upload of crash reports. However, Crashpad does not yet support
    // upload on Android, so this return value currently has no effect and
    // WebView's own uploader will determine consent before uploading. If and
    // when Crashpad supports upload on Android, consent can be determined here,
    // or WebView can continue uploading reports itself.
    return false;
  }

  void GetProductNameAndVersion(std::string* product_name,
                                std::string* version,
                                std::string* channel) override {
    *product_name = "AndroidWebView";
    *version = PRODUCT_VERSION;
    *channel =
        version_info::GetChannelString(version_info::android::GetChannel());
  }

  bool GetCrashDumpLocation(base::FilePath* crash_dir) override {
    return base::PathService::Get(android_webview::DIR_CRASH_DUMPS, crash_dir);
  }

  void GetSanitizationInformation(const char* const** crash_key_allowlist,
                                  void** target_module,
                                  bool* sanitize_stacks) override {
    *crash_key_allowlist = crash_keys::kWebViewCrashKeyAllowList;
#if defined(COMPONENT_BUILD)
    *target_module = nullptr;
#else
    *target_module = reinterpret_cast<void*>(&EnableCrashReporter);
#endif
    *sanitize_stacks = true;
  }

  unsigned int GetCrashDumpPercentage() override { return 100; }

  bool GetBrowserProcessType(std::string* ptype) override {
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kWebViewSandboxedRenderer)) {
      // In single process mode the renderer and browser are in the same
      // process. The process type is "webview" to distinguish this case,
      // and for backwards compatibility.
      *ptype = "webview";
    } else {
      // Otherwise, in multi process mode, "browser" suffices.
      *ptype = "browser";
    }
    return true;
  }

  bool ShouldWriteMinidumpToLog() override { return true; }

  bool JavaExceptionFilter(
      const base::android::JavaRef<jthrowable>& java_exception) {
    return Java_AwCrashReporterClient_stackTraceContainsWebViewCode(
        AttachCurrentThread(), java_exception);
  }

  static AwCrashReporterClient* Get() {
    static base::NoDestructor<AwCrashReporterClient> crash_reporter_client;
    return crash_reporter_client.get();
  }
};

bool g_enabled;

}  // namespace

void EnableCrashReporter(const std::string& process_type) {
  if (g_enabled) {
    NOTREACHED() << "EnableCrashReporter called more than once";
  }

  AwCrashReporterClient* client = AwCrashReporterClient::Get();
  crash_reporter::SetCrashReporterClient(client);
  crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
  if (process_type.empty()) {
    base::android::InitJavaExceptionReporter();
    // Only use the Java exception filter for the main process; in the child,
    // we assume all exceptions are interesting as there is no app code in the
    // process to generate irrelevant exceptions.
    base::android::SetJavaExceptionFilter(base::BindRepeating(
        &AwCrashReporterClient::JavaExceptionFilter, base::Unretained(client)));
  } else {
    base::android::InitJavaExceptionReporterForChildProcess();
  }
  g_enabled = true;
}

bool CrashReporterEnabled() {
  return g_enabled;
}

}  // namespace android_webview
