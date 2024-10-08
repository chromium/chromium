// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef BASE_TRACE_EVENT_BUILTIN_CATEGORIES_H_
#define BASE_TRACE_EVENT_BUILTIN_CATEGORIES_H_

#include <cstddef>
#include <iterator>

#include "base/base_export.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

// List of builtin category names. If you want to use a new category name in
// your code and you get a static assert, this is the right place to register
// the name. If the name is going to be used only for testing, please add it to
// |kCategoriesForTesting| instead.
//
// Since spaces aren't allowed, use '_' to separate words in category names
// (e.g., "content_capture").
//
// Parameter |X| must be a *macro* that takes a single |name| string argument,
// denoting a category name.
#define INTERNAL_TRACE_LIST_BUILTIN_CATEGORIES(X)                        \
  /* These entries must go first to be consistent with the               \
   * CategoryRegistry::kCategory* consts.*/                              \
  X("tracing_categories_exhausted._must_increase_kMaxCategories")        \
  X("tracing_already_shutdown")                                          \
  X("__metadata")                                                        \
  /* The rest of the list is in alphabetical order */                    \
  X("accessibility")                                                     \
  X("AccountFetcherService")                                             \
  X("android.adpf")                                                      \
  X("android.ui.jank")                                                   \
  X("android_webview")                                                   \
  X("android_webview.timeline")                                          \
  /* Actions on Google Hardware, used in Google-internal code. */        \
  X("aogh")                                                              \
  X("audio")                                                             \
  X("base")                                                              \
  X("benchmark")                                                         \
  X("blink")                                                             \
  X("blink.animations")                                                  \
  X("blink.bindings")                                                    \
  X("blink.console")                                                     \
  X("blink.net")                                                         \
  X("blink.resource")                                                    \
  X("blink.user_timing")                                                 \
  X("blink.worker")                                                      \
  X("blink_style")                                                       \
  X("Blob")                                                              \
  X("browser")                                                           \
  X("browsing_data")                                                     \
  X("CacheStorage")                                                      \
  X("Calculators")                                                       \
  X("CameraStream")                                                      \
  X("cppgc")                                                             \
  X("camera")                                                            \
  X("cast_app")                                                          \
  X("cast_perf_test")                                                    \
  X("cast.mdns")                                                         \
  X("cast.mdns.socket")                                                  \
  X("cast.stream")                                                       \
  X("cc")                                                                \
  X("cc.debug")                                                          \
  X("cdp.perf")                                                          \
  X("chromeos")                                                          \
  X("cma")                                                               \
  X("compositor")                                                        \
  X("content")                                                           \
  X("content_capture")                                                   \
  X("interactions")                                                      \
  X("delegated_ink_trails")                                              \
  X("device")                                                            \
  X("devtools")                                                          \
  X("devtools.contrast")                                                 \
  X("devtools.timeline")                                                 \
  X("disk_cache")                                                        \
  X("download")                                                          \
  X("download_service")                                                  \
  X("drm")                                                               \
  X("drmcursor")                                                         \
  X("dwrite")                                                            \
  X("evdev")                                                             \
  X("event")                                                             \
  X("exo")                                                               \
  X("extensions")                                                        \
  X("explore_sites")                                                     \
  X("FileSystem")                                                        \
  X("file_system_provider")                                              \
  X("fledge")                                                            \
  X("fonts")                                                             \
  X("GAMEPAD")                                                           \
  X("gpu")                                                               \
  X("gpu.angle")                                                         \
  X("gpu.angle.texture_metrics")                                         \
  X("gpu.capture")                                                       \
  X("graphics.pipeline")                                                 \
  X("headless")                                                          \
  /* Traces for //components/history. */                                 \
  X("history")                                                           \
  X("hwoverlays")                                                        \
  X("identity")                                                          \
  X("ime")                                                               \
  X("IndexedDB")                                                         \
  X("input")                                                             \
  X("input.scrolling")                                                   \
  X("io")                                                                \
  X("ipc")                                                               \
  X("Java")                                                              \
  X("jni")                                                               \
  X("jpeg")                                                              \
  X("latency")                                                           \
  X("latencyInfo")                                                       \
  X("leveldb")                                                           \
  X("loading")                                                           \
  X("log")                                                               \
  X("login")                                                             \
  X("media")                                                             \
  X("mediastream")                                                       \
  X("media_router")                                                      \
  X("memory")                                                            \
  X("midi")                                                              \
  X("mojom")                                                             \
  X("mus")                                                               \
  X("native")                                                            \
  X("navigation")                                                        \
  X("navigation.debug")                                                  \
  X("net")                                                               \
  X("network.scheduler")                                                 \
  X("netlog")                                                            \
  X("offline_pages")                                                     \
  X("omnibox")                                                           \
  X("oobe")                                                              \
  X("openscreen")                                                        \
  X("ozone")                                                             \
  X("partition_alloc")                                                   \
  X("passwords")                                                         \
  X("p2p")                                                               \
  X("page-serialization")                                                \
  X("paint_preview")                                                     \
  X("pepper")                                                            \
  X("PlatformMalloc")                                                    \
  X("power")                                                             \
  X("ppapi")                                                             \
  X("ppapi_proxy")                                                       \
  X("print")                                                             \
  X("raf_investigation")                                                 \
  X("rail")                                                              \
  X("renderer")                                                          \
  X("renderer_host")                                                     \
  X("renderer.scheduler")                                                \
  X("resources")                                                         \
  X("RLZ")                                                               \
  X("ServiceWorker")                                                     \
  X("SiteEngagement")                                                    \
  X("safe_browsing")                                                     \
  X("scheduler")                                                         \
  X("scheduler.long_tasks")                                              \
  X("screenlock_monitor")                                                \
  X("segmentation_platform")                                             \
  X("sequence_manager")                                                  \
  X("service_manager")                                                   \
  X("sharing")                                                           \
  X("shell")                                                             \
  X("shutdown")                                                          \
  X("skia")                                                              \
  X("sql")                                                               \
  X("stadia_media")                                                      \
  X("stadia_rtc")                                                        \
  X("startup")                                                           \
  X("sync")                                                              \
  X("system_apps")                                                       \
  X("test_gpu")                                                          \
  X("toplevel")                                                          \
  X("toplevel.flow")                                                     \
  X("ui")                                                                \
  X("v8")                                                                \
  X("v8.execute")                                                        \
  X("v8.wasm")                                                           \
  X("ValueStoreFrontend::Backend")                                       \
  X("views")                                                             \
  X("views.frame")                                                       \
  X("viz")                                                               \
  X("vk")                                                                \
  X("wakeup.flow")                                                       \
  X("wayland")                                                           \
  X("webaudio")                                                          \
  X("webengine.fidl")                                                    \
  X("weblayer")                                                          \
  X("WebCore")                                                           \
  X("webnn")                                                             \
  X("webrtc")                                                            \
  X("webrtc_stats")                                                      \
  X("xr")                                                                \
  X(TRACE_DISABLED_BY_DEFAULT("android_view_hierarchy"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("animation-worklet"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("audio"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("audio.latency"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("audio-worklet"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("base"))                                   \
  X(TRACE_DISABLED_BY_DEFAULT("blink.debug"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("blink.debug.display_lock"))               \
  X(TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("blink.debug.layout.trees"))               \
  X(TRACE_DISABLED_BY_DEFAULT("blink.feature_usage"))                    \
  X(TRACE_DISABLED_BY_DEFAULT("blink.image_decoding"))                   \
  X(TRACE_DISABLED_BY_DEFAULT("blink.invalidation"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("identifiability"))                        \
  X(TRACE_DISABLED_BY_DEFAULT("identifiability.high_entropy_api"))       \
  X(TRACE_DISABLED_BY_DEFAULT("cc"))                                     \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug"))                               \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.cdp-perf"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.lcd_text"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.picture"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.frames"))              \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.scheduler.now"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("content.verbose"))                        \
  X(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("cppgc"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.screenshot"))                    \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame"))                \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.inputs"))               \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking")) \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.layers"))               \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"))              \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"))                \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.target-rundown"))                \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown"))             \
  X(TRACE_DISABLED_BY_DEFAULT("devtools.v8-source-rundown-sources"))     \
  X(TRACE_DISABLED_BY_DEFAULT("file"))                                   \
  X(TRACE_DISABLED_BY_DEFAULT("fonts"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("gpu_cmd_queue"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"))                               \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.debug"))                              \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.decoder"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.device"))                             \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.graphite.dawn"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.service"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"))                         \
  X(TRACE_DISABLED_BY_DEFAULT("histogram_samples"))                      \
  X(TRACE_DISABLED_BY_DEFAULT("java-heap-profiler"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("layer-element"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("layout_shift.debug"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("lifecycles"))                             \
  X(TRACE_DISABLED_BY_DEFAULT("loading"))                                \
  X(TRACE_DISABLED_BY_DEFAULT("mediastream"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("memory-infra"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("memory-infra.v8.code_stats"))             \
  X(TRACE_DISABLED_BY_DEFAULT("mojom"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("navigation"))                             \
  X(TRACE_DISABLED_BY_DEFAULT("net"))                                    \
  X(TRACE_DISABLED_BY_DEFAULT("network"))                                \
  X(TRACE_DISABLED_BY_DEFAULT("paint-worklet"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("power"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler.debug"))               \
  X(TRACE_DISABLED_BY_DEFAULT("sequence_manager"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("sequence_manager.debug"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("sequence_manager.verbose_snapshots"))     \
  X(TRACE_DISABLED_BY_DEFAULT("skia"))                                   \
  X(TRACE_DISABLED_BY_DEFAULT("skia.gpu"))                               \
  X(TRACE_DISABLED_BY_DEFAULT("skia.gpu.cache"))                         \
  X(TRACE_DISABLED_BY_DEFAULT("skia.shaders"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("skottie"))                                \
  X(TRACE_DISABLED_BY_DEFAULT("SyncFileSystem"))                         \
  X(TRACE_DISABLED_BY_DEFAULT("system_power"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("system_stats"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("thread_pool_diagnostics"))                \
  X(TRACE_DISABLED_BY_DEFAULT("toplevel.ipc"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("user_action_samples"))                    \
  X(TRACE_DISABLED_BY_DEFAULT("v8.compile"))                             \
  X(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profiler"))                        \
  X(TRACE_DISABLED_BY_DEFAULT("v8.gc"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("v8.gc_stats"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("v8.ic_stats"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("v8.inspector"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("v8.runtime"))                             \
  X(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("v8.runtime_stats_sampling"))              \
  X(TRACE_DISABLED_BY_DEFAULT("v8.stack_trace"))                         \
  X(TRACE_DISABLED_BY_DEFAULT("v8.turbofan"))                            \
  X(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("v8.wasm.turbofan"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"))                \
  X(TRACE_DISABLED_BY_DEFAULT("display.framedisplayed"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("viz.gpu_composite_time"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"))               \
  X(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"))                   \
  X(TRACE_DISABLED_BY_DEFAULT("viz.overdraw"))                           \
  X(TRACE_DISABLED_BY_DEFAULT("viz.quads"))                              \
  X(TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"))                    \
  X(TRACE_DISABLED_BY_DEFAULT("viz.surface_lifetime"))                   \
  X(TRACE_DISABLED_BY_DEFAULT("viz.triangles"))                          \
  X(TRACE_DISABLED_BY_DEFAULT("viz.visual_debugger"))                    \
  X(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"))                     \
  X(TRACE_DISABLED_BY_DEFAULT("webgpu"))                                 \
  X(TRACE_DISABLED_BY_DEFAULT("webnn"))                                  \
  X(TRACE_DISABLED_BY_DEFAULT("webrtc"))                                 \
  X(TRACE_DISABLED_BY_DEFAULT("worker.scheduler"))                       \
  X(TRACE_DISABLED_BY_DEFAULT("xr.debug"))

#define INTERNAL_TRACE_LIST_BUILTIN_CATEGORY_GROUPS(X)                        \
  X("android_webview,toplevel")                                               \
  X("android_webview.timeline,android.ui.jank")                               \
  X("base,toplevel")                                                          \
  X("benchmark,drm")                                                          \
  X("benchmark,latencyInfo,rail")                                             \
  X("benchmark,latencyInfo,rail,input.scrolling")                             \
  X("benchmark,loading")                                                      \
  X("benchmark,rail")                                                         \
  X("benchmark,uma")                                                          \
  X("benchmark,ui")                                                           \
  X("benchmark,viz")                                                          \
  X("benchmark,viz," TRACE_DISABLED_BY_DEFAULT("display.framedisplayed"))     \
  X("blink,benchmark")                                                        \
  X("blink,benchmark,rail," TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"))  \
  X("blink,blink.resource")                                                   \
  X("blink,blink_style")                                                      \
  X("blink,devtools.timeline")                                                \
  X("blink,loading")                                                          \
  X("blink,rail")                                                             \
  X("blink.animations,devtools.timeline,benchmark,rail")                      \
  X("blink.user_timing,rail")                                                 \
  X("browser,content,navigation")                                             \
  X("browser,navigation")                                                     \
  X("browser,navigation,benchmark")                                           \
  X("browser,startup")                                                        \
  X("category1,category2")                                                    \
  X("cc,benchmark")                                                           \
  X("cc,benchmark,input,input.scrolling")                                     \
  X("cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame"))     \
  X("cc,input")                                                               \
  X("cc,raf_investigation")                                                   \
  X("cc," TRACE_DISABLED_BY_DEFAULT("devtools.timeline"))                     \
  X("cc,benchmark," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame"))     \
  X("content,navigation")                                                     \
  X("devtools.timeline,rail")                                                 \
  X("drm,hwoverlays")                                                         \
  X("dwrite,fonts")                                                           \
  X("fonts,ui")                                                               \
  X("gpu,benchmark")                                                          \
  X("gpu,benchmark,android_webview")                                          \
  X("gpu,benchmark,webview")                                                  \
  X("gpu,login")                                                              \
  X("gpu,startup")                                                            \
  X("gpu,toplevel.flow")                                                      \
  X("gpu.angle,startup")                                                      \
  X("inc2,inc")                                                               \
  X("inc,inc2")                                                               \
  X("input,benchmark")                                                        \
  X("input,benchmark,devtools.timeline")                                      \
  X("input,benchmark,devtools.timeline,latencyInfo")                          \
  X("input,benchmark,latencyInfo")                                            \
  X("input,latency")                                                          \
  X("input,rail")                                                             \
  X("input,input.scrolling")                                                  \
  X("input,views")                                                            \
  X("interactions,input.scrolling")                                           \
  X("interactions,startup")                                                   \
  X("ipc,security")                                                           \
  X("ipc,toplevel")                                                           \
  X("Java,devtools," TRACE_DISABLED_BY_DEFAULT("devtools.timeline"))          \
  X("loading,interactions")                                                   \
  X("loading,rail")                                                           \
  X("loading,rail,devtools.timeline")                                         \
  X("login,screenlock_monitor")                                               \
  X("media,gpu")                                                              \
  X("media,rail")                                                             \
  X("navigation,benchmark,rail")                                              \
  X("navigation,rail")                                                        \
  X("renderer,benchmark,rail")                                                \
  X("renderer,benchmark,rail,input.scrolling")                                \
  X("renderer,webkit")                                                        \
  X("renderer_host,navigation")                                               \
  X("renderer_host," TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"))        \
  X("scheduler,devtools.timeline,loading")                                    \
  X("shutdown,viz")                                                           \
  X("startup,benchmark,rail")                                                 \
  X("startup,rail")                                                           \
  X("toplevel,Java")                                                          \
  X("toplevel,viz")                                                           \
  X("ui,input")                                                               \
  X("ui,latency")                                                             \
  X("ui,toplevel")                                                            \
  X("v8," TRACE_DISABLED_BY_DEFAULT("v8.compile"))                            \
  X("v8,devtools.timeline")                                                   \
  X("v8,devtools.timeline," TRACE_DISABLED_BY_DEFAULT("v8.compile"))          \
  X("viz,android.adpf")                                                       \
  X("viz,benchmark")                                                          \
  X("viz,benchmark,graphics.pipeline")                                        \
  X("viz,benchmark,input.scrolling")                                          \
  X("viz,input.scrolling")                                                    \
  X("wakeup.flow,toplevel.flow")                                              \
  X("WebCore,benchmark,rail")                                                 \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug") "," TRACE_DISABLED_BY_DEFAULT(      \
      "viz.quads") "," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.layers")) \
  X(TRACE_DISABLED_BY_DEFAULT("cc.debug.display_items") "," \
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") "," \
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"))                 \
  X(TRACE_DISABLED_BY_DEFAULT("v8.inspector") "," TRACE_DISABLED_BY_DEFAULT(  \
      "v8.stack_trace"))

#define INTERNAL_TRACE_INIT_CATEGORY_NAME(name) name,

#define INTERNAL_TRACE_INIT_CATEGORY(name) {0, 0, name},

PERFETTO_DEFINE_TEST_CATEGORY_PREFIXES("cat",
                                       "foo",
                                       "test",
                                       "kTest",
                                       "noise",
                                       "Testing",
                                       "NotTesting",
                                       TRACE_DISABLED_BY_DEFAULT("test"),
                                       TRACE_DISABLED_BY_DEFAULT("Testing"),
                                       TRACE_DISABLED_BY_DEFAULT("NotTesting"));

#define INTERNAL_CATEGORY(X) perfetto::Category(X),
#define INTERNAL_CATEGORY_GROUP(X) perfetto::Category::Group(X),

// Define a Perfetto TrackEvent data source using the list of categories defined
// above. See https://perfetto.dev/docs/instrumentation/track-events.
PERFETTO_DEFINE_CATEGORIES_IN_NAMESPACE_WITH_ATTRS(
    base,
    BASE_EXPORT,
    INTERNAL_TRACE_LIST_BUILTIN_CATEGORIES(INTERNAL_CATEGORY)
        INTERNAL_TRACE_LIST_BUILTIN_CATEGORY_GROUPS(INTERNAL_CATEGORY_GROUP));
PERFETTO_USE_CATEGORIES_FROM_NAMESPACE(base);

#undef INTERNAL_CATEGORY
#undef INTERNAL_CATEGORY_GROUP

namespace base {
namespace trace_event {

// Constexpr version of string comparison operator. |a| and |b| must be valid
// C-style strings known at compile-time.
constexpr bool StrEqConstexpr(const char* a, const char* b) {
  for (; *a != '\0' && *b != '\0'; ++a, ++b) {
    if (*a != *b)
      return false;
  }
  return *a == *b;
}

// Tests for |StrEqConstexpr()|.
static_assert(StrEqConstexpr("foo", "foo"), "strings should be equal");
static_assert(!StrEqConstexpr("foo", "Foo"), "strings should not be equal");
static_assert(!StrEqConstexpr("foo", "foo1"), "strings should not be equal");
static_assert(!StrEqConstexpr("foo2", "foo"), "strings should not be equal");
static_assert(StrEqConstexpr("", ""), "strings should be equal");
static_assert(!StrEqConstexpr("foo", ""), "strings should not be equal");
static_assert(!StrEqConstexpr("", "foo"), "strings should not be equal");
static_assert(!StrEqConstexpr("ab", "abc"), "strings should not be equal");
static_assert(!StrEqConstexpr("abc", "ab"), "strings should not be equal");

// Static-only class providing access to the compile-time registry of trace
// categories.
// TODO(skyostil): Remove after migrating to the Perfetto client API.
class BASE_EXPORT BuiltinCategories {
 public:
  BuiltinCategories() = delete;
  BuiltinCategories(const BuiltinCategories&) = delete;
  BuiltinCategories& operator=(const BuiltinCategories&) = delete;

  // Returns a built-in category name at |index| in the registry.
  static constexpr const char* At(size_t index) {
    return kBuiltinCategories[index];
  }

  // Returns the amount of built-in categories in the registry.
  static constexpr size_t Size() { return std::size(kBuiltinCategories); }

  // Where in the builtin category list to start when populating the
  // about://tracing UI.
  static constexpr size_t kVisibleCategoryStart = 3;

  // Returns whether the category is either:
  // - Properly registered in the builtin list.
  // - Constists of several categories separated by commas.
  // - Used only in tests.
  // All trace categories are checked against this. A static_assert is triggered
  // if at least one category fails this check.
  static constexpr bool IsAllowedCategory(const char* category) {
#if BUILDFLAG(IS_WIN) && defined(COMPONENT_BUILD)
    return true;
#else
    return IsBuiltinCategory(category) ||
           IsCommaSeparatedCategoryGroup(category) ||
           IsCategoryForTesting(category);
#endif
  }

 private:
  // The array of built-in category names used for compile-time lookup.
  static constexpr const char* kBuiltinCategories[] = {
      INTERNAL_TRACE_LIST_BUILTIN_CATEGORIES(
          INTERNAL_TRACE_INIT_CATEGORY_NAME)};

  // The array of category names used only for testing. It's kept separately
  // from the main list to avoid allocating the space for them in the binary.
  static constexpr const char* kCategoriesForTesting[] = {
      "test_\001\002\003\n\r",
      "test_a",
      "test_all",
      "test_b",
      "test_b1",
      "test_c",
      "test_c0",
      "test_c1",
      "test_c2",
      "test_c3",
      "test_c4",
      "test_tracing",
      "cat",
      "cat1",
      "cat2",
      "cat3",
      "cat4",
      "cat5",
      "cat6",
      "category",
      "test_drink",
      "test_excluded_cat",
      "test_filtered_cat",
      "foo",
      "test_inc",
      "test_inc2",
      "test_included",
      "test_inc_wildcard_",
      "test_inc_wildcard_abc",
      "test_inc_wildchar_bla_end",
      "test_inc_wildchar_x_end",
      "kTestCategory",
      "noise",
      "test_other_included",
      "test",
      "test_category",
      "Testing",
      "TraceEventAgentTestCategory",
      "test_unfiltered_cat",
      "test_x",
      TRACE_DISABLED_BY_DEFAULT("test_c9"),
      TRACE_DISABLED_BY_DEFAULT("test_cat"),
      TRACE_DISABLED_BY_DEFAULT("test_filtered_cat"),
      TRACE_DISABLED_BY_DEFAULT("NotTesting"),
      TRACE_DISABLED_BY_DEFAULT("Testing"),
      TRACE_DISABLED_BY_DEFAULT("test_unfiltered_cat")};

  // Returns whether |str| is in |array| of |array_len|.
  static constexpr bool IsStringInArray(const char* str,
                                        const char* const array[],
                                        size_t array_len) {
    for (size_t i = 0; i < array_len; ++i) {
      if (StrEqConstexpr(str, array[i]))
        return true;
    }
    return false;
  }

  // Returns whether |category_group| contains a ',' symbol, denoting that an
  // event belongs to several categories. We don't add such strings in the
  // builtin list but allow them to pass the static assert.
  static constexpr bool IsCommaSeparatedCategoryGroup(
      const char* category_group) {
    for (; *category_group != '\0'; ++category_group) {
      if (*category_group == ',')
        return true;
    }
    return false;
  }

  // Returns whether |category| is used only for testing.
  static constexpr bool IsCategoryForTesting(const char* category) {
    return IsStringInArray(category, kCategoriesForTesting,
                           std::size(kCategoriesForTesting));
  }

  // Returns whether |category| is registered in the builtin list.
  static constexpr bool IsBuiltinCategory(const char* category) {
    return IsStringInArray(category, kBuiltinCategories,
                           std::size(kBuiltinCategories));
  }
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_BUILTIN_CATEGORIES_H_
