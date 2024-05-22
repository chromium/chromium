// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/trace_event_binding.h"

#include <jni.h>

#include <set>

#include "base/android/jni_string.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/base_tracing.h"
#include "base/tracing_buildflags.h"
#include "build/robolectric_buildflags.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/TraceEvent_jni.h"  // nogncheck
#else
#include "base/tasks_jni/TraceEvent_jni.h"
#endif

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/trace_event_impl.h"  // no-presubmit-check
#include "third_party/perfetto/include/perfetto/tracing/track.h"  // no-presubmit-check nogncheck
#include "third_party/perfetto/protos/perfetto/config/chrome/chrome_config.gen.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {
namespace android {

#if BUILDFLAG(ENABLE_BASE_TRACING)

namespace {

constexpr const char kAndroidViewHierarchyTraceCategory[] =
    TRACE_DISABLED_BY_DEFAULT("android_view_hierarchy");
constexpr const char kAndroidViewHierarchyEventName[] = "AndroidView";

class TraceEnabledObserver : public perfetto::TrackEventSessionObserver {
 public:
  static TraceEnabledObserver* GetInstance() {
    static base::NoDestructor<TraceEnabledObserver> instance;
    return instance.get();
  }

  // perfetto::TrackEventSessionObserver implementation
  void OnSetup(const perfetto::DataSourceBase::SetupArgs& args) override {
    trace_event::TraceConfig trace_config(
        args.config->chrome_config().trace_config());
    event_name_filtering_per_session_[args.internal_instance_index] =
        trace_config.IsEventPackageNameFilterEnabled();
  }

  void OnStart(const perfetto::DataSourceBase::StartArgs&) override {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    base::android::Java_TraceEvent_setEnabled(env, true);
    base::android::Java_TraceEvent_setEventNameFilteringEnabled(
        env, EventNameFilteringEnabled());
  }

  void OnStop(const perfetto::DataSourceBase::StopArgs& args) override {
    event_name_filtering_per_session_.erase(args.internal_instance_index);

    JNIEnv* env = jni_zero::AttachCurrentThread();
    base::android::Java_TraceEvent_setEnabled(
        env, !event_name_filtering_per_session_.empty());
    base::android::Java_TraceEvent_setEventNameFilteringEnabled(
        env, EventNameFilteringEnabled());
  }

 private:
  friend class base::NoDestructor<TraceEnabledObserver>;
  TraceEnabledObserver() = default;
  ~TraceEnabledObserver() override = default;

  // Return true if event name filtering is requested by at least one tracing
  // session.
  bool EventNameFilteringEnabled() const {
    bool event_name_filtering_enabled = false;
    for (const auto& entry : event_name_filtering_per_session_) {
      if (entry.second) {
        event_name_filtering_enabled = true;
      }
    }
    return event_name_filtering_enabled;
  }

  std::unordered_map<uint32_t, bool> event_name_filtering_per_session_;
};


}  // namespace

static void JNI_TraceEvent_RegisterEnabledObserver(JNIEnv* env) {
  base::android::Java_TraceEvent_setEnabled(env, base::TrackEvent::IsEnabled());
  base::TrackEvent::AddSessionObserver(TraceEnabledObserver::GetInstance());
}

static jboolean JNI_TraceEvent_ViewHierarchyDumpEnabled(JNIEnv* env) {
  static const unsigned char* enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          kAndroidViewHierarchyTraceCategory);
  return *enabled;
}

static void JNI_TraceEvent_InitViewHierarchyDump(
    JNIEnv* env,
    jlong id,
    const JavaParamRef<jobject>& obj) {
  TRACE_EVENT(
      kAndroidViewHierarchyTraceCategory, kAndroidViewHierarchyEventName,
      perfetto::TerminatingFlow::ProcessScoped(static_cast<uint64_t>(id)),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* dump = event->set_android_view_dump();
        Java_TraceEvent_dumpViewHierarchy(env, reinterpret_cast<jlong>(dump),
                                          obj);
      });
}

static jlong JNI_TraceEvent_StartActivityDump(JNIEnv* env,
                                              const JavaParamRef<jstring>& name,
                                              jlong dump_proto_ptr) {
  auto* dump = reinterpret_cast<perfetto::protos::pbzero::AndroidViewDump*>(
      dump_proto_ptr);
  auto* activity = dump->add_activity();
  activity->set_name(ConvertJavaStringToUTF8(env, name));
  return reinterpret_cast<jlong>(activity);
}

static void JNI_TraceEvent_AddViewDump(
    JNIEnv* env,
    jint id,
    jint parent_id,
    jboolean is_shown,
    jboolean is_dirty,
    const JavaParamRef<jstring>& class_name,
    const JavaParamRef<jstring>& resource_name,
    jlong activity_proto_ptr) {
  auto* activity = reinterpret_cast<perfetto::protos::pbzero::AndroidActivity*>(
      activity_proto_ptr);
  auto* view = activity->add_view();
  view->set_id(id);
  view->set_parent_id(parent_id);
  view->set_is_shown(is_shown);
  view->set_is_dirty(is_dirty);
  view->set_class_name(ConvertJavaStringToUTF8(env, class_name));
  view->set_resource_name(ConvertJavaStringToUTF8(env, resource_name));
}

#else  // BUILDFLAG(ENABLE_BASE_TRACING)

// Empty implementations when TraceLog isn't available.
static void JNI_TraceEvent_RegisterEnabledObserver(JNIEnv* env) {
  base::android::Java_TraceEvent_setEnabled(env, false);
  // This code should not be reached when base tracing is disabled. Calling
  // setEventNameFilteringEnabled to avoid "unused function" warning.
  base::android::Java_TraceEvent_setEventNameFilteringEnabled(env, false);
}
static jboolean JNI_TraceEvent_ViewHierarchyDumpEnabled(JNIEnv* env) {
  return false;
}
static void JNI_TraceEvent_InitViewHierarchyDump(
    JNIEnv* env,
    jlong id,
    const JavaParamRef<jobject>& obj) {
  DCHECK(false);
  // This code should not be reached when base tracing is disabled. Calling
  // dumpViewHierarchy to avoid "unused function" warning.
  Java_TraceEvent_dumpViewHierarchy(env, 0, obj);
}
static jlong JNI_TraceEvent_StartActivityDump(JNIEnv* env,
                                              const JavaParamRef<jstring>& name,
                                              jlong dump_proto_ptr) {
  return 0;
}
static void JNI_TraceEvent_AddViewDump(
    JNIEnv* env,
    jint id,
    jint parent_id,
    jboolean is_shown,
    jboolean is_dirty,
    const JavaParamRef<jstring>& class_name,
    const JavaParamRef<jstring>& resource_name,
    jlong activity_proto_ptr) {}

#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace {

// Boilerplate for safely converting Java data to TRACE_EVENT data.
class TraceEventDataConverter {
 public:
  TraceEventDataConverter(JNIEnv* env, jstring jarg)
      : has_arg_(jarg != nullptr),
        arg_(jarg ? ConvertJavaStringToUTF8(env, jarg) : "") {}

  TraceEventDataConverter(const TraceEventDataConverter&) = delete;
  TraceEventDataConverter& operator=(const TraceEventDataConverter&) = delete;

  ~TraceEventDataConverter() = default;

  // Return saved values to pass to TRACE_EVENT macros.
  const char* arg_name() { return has_arg_ ? "arg" : nullptr; }
  const std::string& arg() { return arg_; }

 private:
  bool has_arg_;
  std::string arg_;
};

}  // namespace

static void JNI_TraceEvent_Instant(JNIEnv* env,
                                   const JavaParamRef<jstring>& jname,
                                   const JavaParamRef<jstring>& jarg) {
  TraceEventDataConverter converter(env, jarg);

  if (converter.arg_name()) {
    TRACE_EVENT_INSTANT(
        internal::kJavaTraceCategory, nullptr, converter.arg_name(),
        converter.arg(), [&](::perfetto::EventContext& ctx) {
          ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
        });
  } else {
    TRACE_EVENT_INSTANT(
        internal::kJavaTraceCategory, nullptr,
        [&](::perfetto::EventContext& ctx) {
          ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
        });
  }
}

static void JNI_TraceEvent_InstantAndroidIPC(JNIEnv* env,
                                             const JavaParamRef<jstring>& jname,
                                             jlong jdur) {
  TRACE_EVENT_INSTANT(
      internal::kJavaTraceCategory, "AndroidIPC",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* android_ipc = event->set_android_ipc();
        android_ipc->set_name(ConvertJavaStringToUTF8(env, jname));
        android_ipc->set_dur_ms(jdur);
      });
}

#if BUILDFLAG(ENABLE_BASE_TRACING)

static void JNI_TraceEvent_InstantAndroidToolbar(JNIEnv* env,
                                                 jint block_reason,
                                                 jint allow_reason,
                                                 jint snapshot_diff) {
  using AndroidToolbar = perfetto::protos::pbzero::AndroidToolbar;
  TRACE_EVENT_INSTANT(
      internal::kJavaTraceCategory, "AndroidToolbar",
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* android_toolbar = event->set_android_toolbar();
        if (block_reason >= 0) {
          android_toolbar->set_block_capture_reason(
              static_cast<AndroidToolbar::BlockCaptureReason>(block_reason));
        }
        if (allow_reason >= 0) {
          android_toolbar->set_allow_capture_reason(
              static_cast<AndroidToolbar::AllowCaptureReason>(allow_reason));
        }
        if (snapshot_diff >= 0) {
          android_toolbar->set_snapshot_difference(
              static_cast<AndroidToolbar::SnapshotDifference>(snapshot_diff));
        }
      });
}

#else  // BUILDFLAG(ENABLE_BASE_TRACING)

// Empty implementations when TraceLog isn't available.
static void JNI_TraceEvent_InstantAndroidToolbar(JNIEnv* env,
                                                 jint block_reason,
                                                 jint allow_reason,
                                                 jint snapshot_diff) {}

#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

static void JNI_TraceEvent_WebViewStartupTotalFactoryInit(JNIEnv* env,
                                                          jlong start_time_ms,
                                                          jlong duration_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  auto t = perfetto::Track::ThreadScoped(
      reinterpret_cast<void*>(trace_event::GetNextGlobalTraceId()));
  TRACE_EVENT_BEGIN("android_webview.timeline",
                    "WebView.Startup.CreationTime.TotalFactoryInitTime", t,
                    TimeTicks() + Milliseconds(start_time_ms));
  TRACE_EVENT_END("android_webview.timeline", t,
                  TimeTicks() + Milliseconds(start_time_ms + duration_ms));
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_WebViewStartupStage1(JNIEnv* env,
                                                jlong start_time_ms,
                                                jlong duration_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  auto t = perfetto::Track::ThreadScoped(
      reinterpret_cast<void*>(trace_event::GetNextGlobalTraceId()));
  TRACE_EVENT_BEGIN("android_webview.timeline",
                    "WebView.Startup.CreationTime.Stage1.FactoryInit", t,
                    TimeTicks() + Milliseconds(start_time_ms));
  TRACE_EVENT_END("android_webview.timeline", t,
                  TimeTicks() + Milliseconds(start_time_ms + duration_ms));
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_WebViewStartupStage2(JNIEnv* env,
                                                jlong start_time_ms,
                                                jlong duration_ms,
                                                jboolean is_cold_startup) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  auto t = perfetto::Track::ThreadScoped(
      reinterpret_cast<void*>(trace_event::GetNextGlobalTraceId()));
  if (is_cold_startup) {
    TRACE_EVENT_BEGIN("android_webview.timeline",
                      "WebView.Startup.CreationTime.Stage2.ProviderInit.Cold",
                      t, TimeTicks() + Milliseconds(start_time_ms));
  } else {
    TRACE_EVENT_BEGIN("android_webview.timeline",
                      "WebView.Startup.CreationTime.Stage2.ProviderInit.Warm",
                      t, TimeTicks() + Milliseconds(start_time_ms));
  }

  TRACE_EVENT_END("android_webview.timeline", t,
                  TimeTicks() + Milliseconds(start_time_ms + duration_ms));
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_WebViewStartupStartChromiumLocked(
    JNIEnv* env,
    jlong start_time_ms,
    jlong duration_ms,
    jint call_site,
    jboolean from_ui_thread) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  auto t = perfetto::Track::ThreadScoped(
      reinterpret_cast<void*>(trace_event::GetNextGlobalTraceId()));
  TRACE_EVENT_BEGIN(
      "android_webview.timeline",
      "WebView.Startup.CreationTime.StartChromiumLocked", t,
      TimeTicks() + Milliseconds(start_time_ms),
      [&](perfetto::EventContext ctx) {
        auto* webview_startup =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_webview_startup();
        webview_startup->set_from_ui_thread((bool)from_ui_thread);
        webview_startup->set_call_site(
            (perfetto::protos::pbzero::perfetto_pbzero_enum_WebViewStartup::
                 CallSite)call_site);
      });
  TRACE_EVENT_END("android_webview.timeline", t,
                  TimeTicks() + Milliseconds(start_time_ms + duration_ms));
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_StartupActivityStart(JNIEnv* env,
                                                jlong activity_id,
                                                jlong start_time_ms) {
  TRACE_EVENT_INSTANT(
      "interactions", "Startup.ActivityStart",
      TimeTicks() + Milliseconds(start_time_ms),
      [&](perfetto::EventContext ctx) {
        auto* start_up = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                             ->set_startup();
        start_up->set_activity_id(activity_id);
      });
}

static void JNI_TraceEvent_StartupLaunchCause(JNIEnv* env,
                                              jlong activity_id,
                                              jlong start_time_ms,
                                              jint cause) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  using Startup = perfetto::protos::pbzero::StartUp;
  auto launchType = Startup::OTHER;
  switch (cause) {
    case Startup::CUSTOM_TAB:
      launchType = Startup::CUSTOM_TAB;
      break;
    case Startup::TWA:
      launchType = Startup::TWA;
      break;
    case Startup::RECENTS:
      launchType = Startup::RECENTS;
      break;
    case Startup::RECENTS_OR_BACK:
      launchType = Startup::RECENTS_OR_BACK;
      break;
    case Startup::FOREGROUND_WHEN_LOCKED:
      launchType = Startup::FOREGROUND_WHEN_LOCKED;
      break;
    case Startup::MAIN_LAUNCHER_ICON:
      launchType = Startup::MAIN_LAUNCHER_ICON;
      break;
    case Startup::MAIN_LAUNCHER_ICON_SHORTCUT:
      launchType = Startup::MAIN_LAUNCHER_ICON_SHORTCUT;
      break;
    case Startup::HOME_SCREEN_WIDGET:
      launchType = Startup::HOME_SCREEN_WIDGET;
      break;
    case Startup::OPEN_IN_BROWSER_FROM_MENU:
      launchType = Startup::OPEN_IN_BROWSER_FROM_MENU;
      break;
    case Startup::EXTERNAL_SEARCH_ACTION_INTENT:
      launchType = Startup::EXTERNAL_SEARCH_ACTION_INTENT;
      break;
    case Startup::NOTIFICATION:
      launchType = Startup::NOTIFICATION;
      break;
    case Startup::EXTERNAL_VIEW_INTENT:
      launchType = Startup::EXTERNAL_VIEW_INTENT;
      break;
    case Startup::OTHER_CHROME:
      launchType = Startup::OTHER_CHROME;
      break;
    case Startup::WEBAPK_CHROME_DISTRIBUTOR:
      launchType = Startup::WEBAPK_CHROME_DISTRIBUTOR;
      break;
    case Startup::WEBAPK_OTHER_DISTRIBUTOR:
      launchType = Startup::WEBAPK_OTHER_DISTRIBUTOR;
      break;
    case Startup::HOME_SCREEN_SHORTCUT:
      launchType = Startup::HOME_SCREEN_SHORTCUT;
      break;
    case Startup::SHARE_INTENT:
      launchType = Startup::SHARE_INTENT;
      break;
    case Startup::NFC:
      launchType = Startup::NFC;
      break;
    default:
      break;
  }

  TRACE_EVENT_INSTANT(
      "interactions,startup", "Startup.LaunchCause",
      TimeTicks() + Milliseconds(start_time_ms),
      [&](perfetto::EventContext ctx) {
        auto* start_up = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                             ->set_startup();
        start_up->set_activity_id(activity_id);
        start_up->set_launch_cause(launchType);
      });
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_StartupTimeToFirstVisibleContent2(
    JNIEnv* env,
    jlong activity_id,
    jlong start_time_ms,
    jlong duration_ms) {
#if BUILDFLAG(ENABLE_BASE_TRACING)
  [[maybe_unused]] const perfetto::Track track(
      base::trace_event::GetNextGlobalTraceId(),
      perfetto::ProcessTrack::Current());
  TRACE_EVENT_BEGIN(
      "interactions,startup", "Startup.TimeToFirstVisibleContent2", track,
      TimeTicks() + Milliseconds(start_time_ms),
      [&](perfetto::EventContext ctx) {
        auto* start_up = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                             ->set_startup();
        start_up->set_activity_id(activity_id);
      });

  TRACE_EVENT_END("interactions,startup", track,
                  TimeTicks() + Milliseconds(start_time_ms + duration_ms));
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)
}

static void JNI_TraceEvent_Begin(JNIEnv* env,
                                 const JavaParamRef<jstring>& jname,
                                 const JavaParamRef<jstring>& jarg) {
  TraceEventDataConverter converter(env, jarg);
  if (converter.arg_name()) {
    TRACE_EVENT_BEGIN(
        internal::kJavaTraceCategory, nullptr, converter.arg_name(),
        converter.arg(), [&](::perfetto::EventContext& ctx) {
          ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
        });
  } else {
    TRACE_EVENT_BEGIN(
        internal::kJavaTraceCategory, nullptr,
        [&](::perfetto::EventContext& ctx) {
          ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
        });
  }
}

static void JNI_TraceEvent_BeginWithIntArg(JNIEnv* env,
                                           const JavaParamRef<jstring>& jname,
                                           jint jarg) {
  TRACE_EVENT_BEGIN(
      internal::kJavaTraceCategory, nullptr, "arg", jarg,
      [&](::perfetto::EventContext& ctx) {
        ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
      });
}

static void JNI_TraceEvent_End(JNIEnv* env,
                               const JavaParamRef<jstring>& jarg,
                               jlong jflow) {
  TraceEventDataConverter converter(env, jarg);
  bool has_arg = converter.arg_name();
  bool has_flow = jflow != 0;
  if (has_arg && has_flow) {
    TRACE_EVENT_END(internal::kJavaTraceCategory,
                    perfetto::Flow::ProcessScoped(static_cast<uint64_t>(jflow)),
                    converter.arg_name(), converter.arg());
  } else if (has_arg) {
    TRACE_EVENT_END(internal::kJavaTraceCategory, converter.arg_name(),
                    converter.arg());
  } else if (has_flow) {
    TRACE_EVENT_END(
        internal::kJavaTraceCategory,
        perfetto::Flow::ProcessScoped(static_cast<uint64_t>(jflow)));
  } else {
    TRACE_EVENT_END(internal::kJavaTraceCategory);
  }
}

static void JNI_TraceEvent_BeginToplevel(JNIEnv* env,
                                         const JavaParamRef<jstring>& jtarget) {
  TRACE_EVENT_BEGIN(
      internal::kToplevelTraceCategory, nullptr,
      [&](::perfetto::EventContext& ctx) {
        ctx.event()->set_name(ConvertJavaStringToUTF8(env, jtarget));
      });
}

static void JNI_TraceEvent_EndToplevel(JNIEnv* env) {
  TRACE_EVENT_END(internal::kToplevelTraceCategory);
}

static void JNI_TraceEvent_StartAsync(JNIEnv* env,
                                      const JavaParamRef<jstring>& jname,
                                      jlong jid) {
  TRACE_EVENT_BEGIN(
      internal::kJavaTraceCategory, nullptr,
      perfetto::Track(static_cast<uint64_t>(jid)),
      [&](::perfetto::EventContext& ctx) {
        ctx.event()->set_name(ConvertJavaStringToUTF8(env, jname));
      });
}

static void JNI_TraceEvent_FinishAsync(JNIEnv* env,
                                       jlong jid) {
  TRACE_EVENT_END(internal::kJavaTraceCategory,
                  perfetto::Track(static_cast<uint64_t>(jid)));
}

}  // namespace android
}  // namespace base
