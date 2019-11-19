// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/process_memory_metrics_emitter.h"

#include <set>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/trace_event_analyzer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/network_service_util.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/features.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/test/background_page_watcher.h"
#include "extensions/test/test_extension_dir.h"
#endif

namespace {

using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpType;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::mojom::ProcessType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::BackgroundPageWatcher;
using extensions::Extension;
using extensions::ProcessManager;
using extensions::TestExtensionDir;
#endif

using UkmEntry = ukm::builders::Memory_Experimental;

// Whether the value of a metric can be zero.
enum class ValueRestriction { NONE, ABOVE_ZERO };

// Returns the number of renderers associated with top-level frames in
// |browser|. There can be other renderers in the process (e.g. spare renderer).
int GetNumRenderers(Browser* browser) {
  // Since multiple tabs can be hosted in the same process, RenderProcessHosts
  // need to be deduped.
  std::set<content::RenderProcessHost*> render_process_hosts;
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    render_process_hosts.insert(browser->tab_strip_model()
                                    ->GetWebContentsAt(i)
                                    ->GetSiteInstance()
                                    ->GetProcess());
  }
  return static_cast<int>(render_process_hosts.size());
}

void RequestGlobalDumpCallback(base::Closure quit_closure,
                               bool success,
                               uint64_t) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure);
  ASSERT_TRUE(success);
}

void OnStartTracingDoneCallback(
    base::trace_event::MemoryDumpLevelOfDetail explicit_dump_type,
    base::Closure quit_closure) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDumpAndAppendToTrace(
          MemoryDumpType::EXPLICITLY_TRIGGERED, explicit_dump_type,
          MemoryDumpDeterminism::NONE,
          Bind(&RequestGlobalDumpCallback, quit_closure));
}

class ProcessMemoryMetricsEmitterFake : public ProcessMemoryMetricsEmitter {
 public:
  explicit ProcessMemoryMetricsEmitterFake(base::RunLoop* run_loop,
                                           ukm::TestUkmRecorder* recorder)
      : run_loop_(run_loop), recorder_(recorder) {}

 private:
  ~ProcessMemoryMetricsEmitterFake() override {}

  void ReceivedMemoryDump(bool success,
                          std::unique_ptr<GlobalMemoryDump> ptr) override {
    EXPECT_TRUE(success);
    ProcessMemoryMetricsEmitter::ReceivedMemoryDump(success, std::move(ptr));
    finished_memory_dump_ = true;
    QuitIfFinished();
  }

  void ReceivedProcessInfos(std::vector<ProcessInfo> process_infos) override {
    ProcessMemoryMetricsEmitter::ReceivedProcessInfos(std::move(process_infos));
    finished_process_info_ = true;
    QuitIfFinished();
  }

  void QuitIfFinished() {
    if (!finished_memory_dump_ || !finished_process_info_)
      return;
    if (run_loop_)
      run_loop_->Quit();
  }

  ukm::UkmRecorder* GetUkmRecorder() override { return recorder_; }

  base::RunLoop* run_loop_;
  bool finished_memory_dump_ = false;
  bool finished_process_info_ = false;
  ukm::TestUkmRecorder* recorder_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterFake);
};

void CheckMemoryMetric(const std::string& name,
                       const base::HistogramTester& histogram_tester,
                       int count,
                       ValueRestriction value_restriction,
                       int number_of_processes = 1u) {
  std::unique_ptr<base::HistogramSamples> samples(
      histogram_tester.GetHistogramSamplesSinceCreation(name));
  ASSERT_TRUE(samples);

  if (name.find("Renderer") != std::string::npos) {
    // There can be a spare renderer not accounted for in |number_of_processes|.
    EXPECT_GE(samples->TotalCount(), count * number_of_processes) << name;
    EXPECT_LE(samples->TotalCount(), count * (number_of_processes + 1)) << name;
  } else {
    EXPECT_EQ(samples->TotalCount(), count * number_of_processes) << name;
  }

  if (count != 0 && number_of_processes != 0 &&
      value_restriction == ValueRestriction::ABOVE_ZERO)
    EXPECT_GT(samples->sum(), 0u) << name;

  // As a sanity check, no memory stat should exceed 4 GB.
  int64_t maximum_expected_size = 1ll << 32;
  EXPECT_LT(samples->sum(), maximum_expected_size) << name;
}

void CheckExperimentalMemoryMetricsForProcessType(
    const base::HistogramTester& histogram_tester,
    int count,
    const char* process_type,
    int number_of_processes) {
#if !defined(OS_WIN)
  CheckMemoryMetric(
      std::string("Memory.Experimental.") + process_type + "2.Malloc",
      histogram_tester, count, ValueRestriction::ABOVE_ZERO,
      number_of_processes);
#endif
  CheckMemoryMetric(
      std::string("Memory.Experimental.") + process_type + "2.BlinkGC",
      histogram_tester, count, ValueRestriction::NONE, number_of_processes);
  CheckMemoryMetric(
      std::string("Memory.Experimental.") + process_type + "2.PartitionAlloc",
      histogram_tester, count, ValueRestriction::NONE, number_of_processes);
  // V8 memory footprint can be below 1 MB, which is reported as zero.
  CheckMemoryMetric(std::string("Memory.Experimental.") + process_type + "2.V8",
                    histogram_tester, count, ValueRestriction::NONE,
                    number_of_processes);
}

void CheckExperimentalMemoryMetrics(
    const base::HistogramTester& histogram_tester,
    int count,
    int number_of_renderer_processes,
    int number_of_extension_processes) {
#if !defined(OS_WIN)
  CheckMemoryMetric("Memory.Experimental.Browser2.Malloc", histogram_tester,
                    count, ValueRestriction::ABOVE_ZERO);
#endif
  if (number_of_renderer_processes) {
    CheckExperimentalMemoryMetricsForProcessType(
        histogram_tester, count, "Renderer", number_of_renderer_processes);
  }
  if (number_of_extension_processes) {
    CheckExperimentalMemoryMetricsForProcessType(
        histogram_tester, count, "Extension", number_of_extension_processes);
  }
  CheckMemoryMetric("Memory.Experimental.Total2.PrivateMemoryFootprint",
                    histogram_tester, count, ValueRestriction::ABOVE_ZERO);
}

void CheckStableMemoryMetrics(const base::HistogramTester& histogram_tester,
                              int count,
                              int number_of_renderer_processes,
                              int number_of_extension_processes) {
  const int count_for_resident_set =
#if defined(OS_MACOSX)
      0;
#else
      count;
#endif
  const int count_for_private_swap_footprint =
#if defined(OS_LINUX) || defined(OS_ANDROID)
      count;
#else
      0;
#endif

  if (number_of_renderer_processes) {
    CheckMemoryMetric("Memory.Renderer.ResidentSet", histogram_tester,
                      count_for_resident_set, ValueRestriction::ABOVE_ZERO,
                      number_of_renderer_processes);
    CheckMemoryMetric("Memory.Renderer.PrivateMemoryFootprint",
                      histogram_tester, count, ValueRestriction::ABOVE_ZERO,
                      number_of_renderer_processes);
    // Shared memory footprint can be below 1 MB, which is reported as zero.
    CheckMemoryMetric("Memory.Renderer.SharedMemoryFootprint", histogram_tester,
                      count, ValueRestriction::NONE,
                      number_of_renderer_processes);
    CheckMemoryMetric("Memory.Renderer.PrivateSwapFootprint", histogram_tester,
                      count_for_private_swap_footprint, ValueRestriction::NONE,
                      number_of_renderer_processes);
  }

  if (number_of_extension_processes) {
    CheckMemoryMetric("Memory.Extension.ResidentSet", histogram_tester,
                      count_for_resident_set, ValueRestriction::ABOVE_ZERO,
                      number_of_extension_processes);
    CheckMemoryMetric("Memory.Extension.PrivateMemoryFootprint",
                      histogram_tester, count, ValueRestriction::ABOVE_ZERO,
                      number_of_extension_processes);
    // Shared memory footprint can be below 1 MB, which is reported as zero.
    CheckMemoryMetric("Memory.Extension.SharedMemoryFootprint",
                      histogram_tester, count, ValueRestriction::NONE,
                      number_of_extension_processes);
    CheckMemoryMetric("Memory.Extension.PrivateSwapFootprint", histogram_tester,
                      count_for_private_swap_footprint, ValueRestriction::NONE,
                      number_of_extension_processes);
  }

  int number_of_ns_processes = content::IsOutOfProcessNetworkService() ? 1 : 0;
  CheckMemoryMetric("Memory.NetworkService.ResidentSet", histogram_tester,
                    count_for_resident_set, ValueRestriction::ABOVE_ZERO,
                    number_of_ns_processes);
  CheckMemoryMetric("Memory.NetworkService.PrivateMemoryFootprint",
                    histogram_tester, count, ValueRestriction::ABOVE_ZERO,
                    number_of_ns_processes);
  // Shared memory footprint can be below 1 MB, which is reported as zero.
  CheckMemoryMetric("Memory.NetworkService.SharedMemoryFootprint",
                    histogram_tester, count, ValueRestriction::NONE,
                    number_of_ns_processes);
  CheckMemoryMetric("Memory.NetworkService.PrivateSwapFootprint",
                    histogram_tester, count_for_private_swap_footprint,
                    ValueRestriction::NONE, number_of_ns_processes);

  CheckMemoryMetric("Memory.Total.ResidentSet", histogram_tester,
                    count_for_resident_set, ValueRestriction::ABOVE_ZERO);
  CheckMemoryMetric("Memory.Total.PrivateMemoryFootprint", histogram_tester,
                    count, ValueRestriction::ABOVE_ZERO);
  CheckMemoryMetric("Memory.Total.RendererPrivateMemoryFootprint",
                    histogram_tester, count, ValueRestriction::ABOVE_ZERO);
  // Shared memory footprint can be below 1 MB, which is reported as zero.
  CheckMemoryMetric("Memory.Total.SharedMemoryFootprint", histogram_tester,
                    count, ValueRestriction::NONE);
}

void CheckAllMemoryMetrics(const base::HistogramTester& histogram_tester,
                           int count,
                           int number_of_renderer_processes = 1u,
                           int number_of_extension_processes = 0u) {
  CheckExperimentalMemoryMetrics(histogram_tester, count,
                                 number_of_renderer_processes,
                                 number_of_extension_processes);
  CheckStableMemoryMetrics(histogram_tester, count,
                           number_of_renderer_processes,
                           number_of_extension_processes);
}

}  // namespace

class ProcessMemoryMetricsEmitterTest
    : public extensions::ExtensionBrowserTest {
 public:
  ProcessMemoryMetricsEmitterTest() {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmFeature);
  }

  ~ProcessMemoryMetricsEmitterTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;

  void CheckExactMetricWithName(const ukm::mojom::UkmEntry* entry,
                                const char* name,
                                int64_t expected_value) {
    const int64_t* value = ukm::TestUkmRecorder::GetEntryMetric(entry, name);
    ASSERT_TRUE(value) << name;
    EXPECT_EQ(expected_value, *value) << name;
  }

  void CheckMemoryMetricWithName(
      const ukm::mojom::UkmEntry* entry,
      const char* name,
      ValueRestriction value_restriction = ValueRestriction::NONE) {
    const int64_t* value = ukm::TestUkmRecorder::GetEntryMetric(entry, name);
    ASSERT_TRUE(value) << name;

    switch (value_restriction) {
      case ValueRestriction::NONE:
        EXPECT_GE(*value, 0) << name;
        break;
      case ValueRestriction::ABOVE_ZERO:
        EXPECT_GT(*value, 0) << name;
        break;
    }

    EXPECT_LE(*value, 4000) << name;
  }

  void CheckTimeMetricWithName(const ukm::mojom::UkmEntry* entry,
                               const char* name) {
    const int64_t* value = ukm::TestUkmRecorder::GetEntryMetric(entry, name);
    ASSERT_TRUE(value) << name;
    EXPECT_GE(*value, 0) << name;
    EXPECT_LE(*value, 300) << name;
  }

  void CheckAllUkmEntries(size_t entry_count = 1u) {
    const auto& entries =
        test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    size_t browser_entry_count = 0;
    size_t renderer_entry_count = 0;
    size_t total_entry_count = 0;

    for (const auto* entry : entries) {
      if (ProcessHasTypeForEntry(entry, ProcessType::BROWSER)) {
        browser_entry_count++;
        CheckUkmBrowserEntry(entry);
      } else if (ProcessHasTypeForEntry(entry, ProcessType::RENDERER)) {
        renderer_entry_count++;
        CheckUkmRendererEntry(entry);
      } else if (ProcessHasTypeForEntry(entry, ProcessType::GPU)) {
        CheckUkmGPUEntry(entry);
      } else if (ProcessHasTypeForEntry(entry, ProcessType::UTILITY)) {
        // No expectations.
      } else {
        // This must be Total2.
        total_entry_count++;
        CheckMemoryMetricWithName(entry,
                                  UkmEntry::kTotal2_PrivateMemoryFootprintName,
                                  ValueRestriction::ABOVE_ZERO);
      }
    }
    EXPECT_EQ(entry_count, browser_entry_count);
    EXPECT_EQ(entry_count, total_entry_count);

    EXPECT_GE(renderer_entry_count, entry_count);
  }

  void CheckUkmRendererEntry(const ukm::mojom::UkmEntry* entry) {
#if !defined(OS_WIN)
    CheckMemoryMetricWithName(entry, UkmEntry::kMallocName,
                              ValueRestriction::ABOVE_ZERO);
#endif
#if !defined(OS_MACOSX)
    CheckMemoryMetricWithName(entry, UkmEntry::kResidentName,
                              ValueRestriction::ABOVE_ZERO);
#endif
    CheckMemoryMetricWithName(entry, UkmEntry::kPrivateMemoryFootprintName,
                              ValueRestriction::ABOVE_ZERO);
    CheckMemoryMetricWithName(entry, UkmEntry::kBlinkGCName,
                              ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kPartitionAllocName,
                              ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kV8Name, ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kNumberOfExtensionsName,
                              ValueRestriction::NONE);
    CheckTimeMetricWithName(entry, UkmEntry::kUptimeName);

    CheckMemoryMetricWithName(entry, UkmEntry::kNumberOfDocumentsName,
                              ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kNumberOfFramesName,
                              ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kNumberOfLayoutObjectsName,
                              ValueRestriction::NONE);
    CheckMemoryMetricWithName(entry, UkmEntry::kNumberOfNodesName,
                              ValueRestriction::NONE);
  }

  void CheckUkmBrowserEntry(const ukm::mojom::UkmEntry* entry) {
#if !defined(OS_WIN)
    CheckMemoryMetricWithName(entry, UkmEntry::kMallocName,
                              ValueRestriction::ABOVE_ZERO);
#endif
#if !defined(OS_MACOSX)
    CheckMemoryMetricWithName(entry, UkmEntry::kResidentName,
                              ValueRestriction::ABOVE_ZERO);
#endif
    CheckMemoryMetricWithName(entry, UkmEntry::kPrivateMemoryFootprintName,
                              ValueRestriction::ABOVE_ZERO);

    CheckTimeMetricWithName(entry, UkmEntry::kUptimeName);
  }

  void CheckUkmGPUEntry(const ukm::mojom::UkmEntry* entry) {
    CheckTimeMetricWithName(entry, UkmEntry::kUptimeName);
  }

  bool ProcessHasTypeForEntry(const ukm::mojom::UkmEntry* entry,
                              ProcessType process_type) {
    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, UkmEntry::kProcessTypeName);
    return value && *value == static_cast<int64_t>(process_type);
  }

  void CheckPageInfoUkmMetrics(GURL url,
                               bool is_visible,
                               size_t entry_count = 1u) {
    const auto& entries =
        test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    size_t found_count = false;
    const ukm::mojom::UkmEntry* last_entry = nullptr;
    for (const auto* entry : entries) {
      const ukm::UkmSource* source =
          test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (!source || source->url() != url)
        continue;
      if (!test_ukm_recorder_->EntryHasMetric(entry, UkmEntry::kIsVisibleName))
        continue;
      found_count++;
      last_entry = entry;
      EXPECT_TRUE(ProcessHasTypeForEntry(entry, ProcessType::RENDERER));
      CheckTimeMetricWithName(entry, UkmEntry::kTimeSinceLastNavigationName);
      CheckTimeMetricWithName(entry,
                              UkmEntry::kTimeSinceLastVisibilityChangeName);
    }
    CheckExactMetricWithName(last_entry, UkmEntry::kIsVisibleName, is_visible);
    EXPECT_EQ(entry_count, found_count);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Create an barebones extension with a background page for the given name.
  const Extension* CreateExtension(const std::string& name) {
    auto dir = std::make_unique<TestExtensionDir>();
    dir->WriteManifestWithSingleQuotes(
        base::StringPrintf("{"
                           "'name': '%s',"
                           "'version': '1',"
                           "'manifest_version': 2,"
                           "'background': {'page': 'bg.html'}"
                           "}",
                           name.c_str()));
    dir->WriteFile(FILE_PATH_LITERAL("bg.html"), "");

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  const Extension* CreateHostedApp(const std::string& name,
                                   const GURL& app_url) {
    std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir);
    dir->WriteManifestWithSingleQuotes(base::StringPrintf(
        "{"
        "'name': '%s',"
        "'version': '1',"
        "'manifest_version': 2,"
        "'app': {'urls': ['%s'], 'launch': {'web_url': '%s'}}"
        "}",
        name.c_str(), app_url.spec().c_str(), app_url.spec().c_str()));

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

#endif

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ProcessMemoryMetricsEmitterTest);
};

// TODO(crbug.com/732501): Re-enable on Win once not flaky.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || defined(OS_WIN)
#define MAYBE_FetchAndEmitMetrics DISABLED_FetchAndEmitMetrics
#else
#define MAYBE_FetchAndEmitMetrics FetchAndEmitMetrics
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetrics) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers);
  CheckAllUkmEntries();
  CheckPageInfoUkmMetrics(url, true);
}

// TODO(https://crbug.com/990148): Re-enable on Win and Linux once not flaky.
#if BUILDFLAG(ENABLE_EXTENSIONS)
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_FetchAndEmitMetricsWithExtensions \
  DISABLED_FetchAndEmitMetricsWithExtensions
#else
#define MAYBE_FetchAndEmitMetricsWithExtensions \
  FetchAndEmitMetricsWithExtensions
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetricsWithExtensions) {
  const Extension* extension1 = CreateExtension("Extension 1");
  const Extension* extension2 = CreateExtension("Extension 2");
  ProcessManager* pm = ProcessManager::Get(profile());

  // Verify that the extensions has loaded.
  BackgroundPageWatcher(pm, extension1).WaitForOpen();
  BackgroundPageWatcher(pm, extension2).WaitForOpen();
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));
  constexpr int kNumExtensionProcesses = 2;

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers,
                        kNumExtensionProcesses);
  // Extension processes do not have page_info.
  CheckAllUkmEntries();
  CheckPageInfoUkmMetrics(url, true);
}

#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_FetchAndEmitMetricsWithHostedApps \
  DISABLED_FetchAndEmitMetricsWithHostedApps
#else
// TODO(crbug.com/943207): Re-enable this test once it's not flaky anymore.
#define MAYBE_FetchAndEmitMetricsWithHostedApps \
  DISABLED_FetchAndEmitMetricsWithHostedApps
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchAndEmitMetricsWithHostedApps) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url = embedded_test_server()->GetURL("app.org", "/empty.html");
  const Extension* app = CreateHostedApp("App", GURL("http://app.org"));
  ui_test_utils::NavigateToURL(browser(), app_url);

  // Verify that the hosted app has loaded.
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(app->id()).size());

  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));
  constexpr int kNumExtensionProcesses = 0;

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers,
                        kNumExtensionProcesses);
  CheckAllUkmEntries();
  CheckPageInfoUkmMetrics(url, true);
}

IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       FetchAndEmitMetricsWithExtensionsAndHostReuse) {
  // This test does not work with --site-per-process flag since this test
  // combines multiple extensions in the same process.
  if (content::AreAllSitesIsolatedForTesting())
    return;
  // Limit the number of renderer processes to force reuse.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  const Extension* extension1 = CreateExtension("Extension 1");
  const Extension* extension2 = CreateExtension("Extension 2");
  ProcessManager* pm = ProcessManager::Get(profile());

  // Verify that the extensions has loaded.
  BackgroundPageWatcher(pm, extension1).WaitForOpen();
  BackgroundPageWatcher(pm, extension2).WaitForOpen();
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  // Intentionally let emitter leave scope to check that it correctly keeps
  // itself alive.
  {
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));
  constexpr int kNumExtensionProcesses = 1;

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers,
                        kNumExtensionProcesses);
  CheckAllUkmEntries();
  // When hosts share a process, no unique URL is identified, therefore no page
  // info.
  const auto& entries =
      test_ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
  for (const auto* entry : entries) {
    EXPECT_EQ(nullptr,
              test_ukm_recorder_->GetSourceForSourceId(entry->source_id));
  }
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// TODO(crbug.com/989810): Re-enable on Win once not flaky.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || defined(OS_WIN)
#define MAYBE_FetchDuringTrace DISABLED_FetchDuringTrace
#else
#define MAYBE_FetchDuringTrace FetchDuringTrace
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_FetchDuringTrace) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;

  {
    base::RunLoop run_loop;

    base::trace_event::TraceConfig trace_config(
        base::trace_event::TraceConfigMemoryTestUtil::
            GetTraceConfig_EmptyTriggers());
    ASSERT_TRUE(tracing::BeginTracingWithTraceConfig(
        trace_config, Bind(&OnStartTracingDoneCallback,
                           base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
                           run_loop.QuitClosure())));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();

    run_loop.Run();
  }

  std::string json_events;
  ASSERT_TRUE(tracing::EndTracing(&json_events));

  trace_analyzer::TraceEventVector events;
  std::unique_ptr<trace_analyzer::TraceAnalyzer> analyzer(
      trace_analyzer::TraceAnalyzer::Create(json_events));
  analyzer->FindEvents(
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_MEMORY_DUMP),
      &events);

  ASSERT_GT(events.size(), 1u);
  ASSERT_TRUE(trace_analyzer::CountMatches(
      events, trace_analyzer::Query::EventNameIs(MemoryDumpTypeToString(
                  MemoryDumpType::EXPLICITLY_TRIGGERED))));

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers);
  CheckAllUkmEntries();
  CheckPageInfoUkmMetrics(url, true);
}

// Flaky test: https://crbug.com/731466
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       DISABLED_FetchThreeTimes) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("foo.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;

  int count = 3;
  for (int i = 0; i < count; ++i) {
    // Only the last emitter should stop the run loop.
    auto emitter = base::MakeRefCounted<ProcessMemoryMetricsEmitterFake>(
        (i == count - 1) ? &run_loop : nullptr, test_ukm_recorder_.get());
    emitter->FetchAndEmitProcessMemoryMetrics();
  }

  run_loop.Run();

  constexpr int kNumRenderers = 2;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));

  CheckAllMemoryMetrics(histogram_tester, count, kNumRenderers);
  CheckAllUkmEntries(count);
  CheckPageInfoUkmMetrics(url, true, count);
}

// Test is flaky on chromeos and linux. https://crbug.com/938054.
// Test is flaky on mac and win: https://crbug.com/948674.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) ||         \
    defined(OS_CHROMEOS) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_WIN)
#define MAYBE_ForegroundAndBackgroundPages DISABLED_ForegroundAndBackgroundPages
#else
#define MAYBE_ForegroundAndBackgroundPages ForegroundAndBackgroundPages
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest,
                       MAYBE_ForegroundAndBackgroundPages) {
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("a.com", "/empty.html");
  const GURL url2 = embedded_test_server()->GetURL("b.com", "/empty.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* tab1 = add_tab.Wait();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab2;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* tab2 = add_tab2.Wait();

  base::HistogramTester histogram_tester;
  {
    base::RunLoop run_loop;
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
    run_loop.Run();
  }

  constexpr int kNumRenderers = 3;
  EXPECT_EQ(kNumRenderers, GetNumRenderers(browser()));

  CheckAllMemoryMetrics(histogram_tester, 1, kNumRenderers);
  CheckAllUkmEntries();
  CheckPageInfoUkmMetrics(url1, true /* is_visible */);
  CheckPageInfoUkmMetrics(url2, false /* is_visible */);

  tab1->WasHidden();
  tab2->WasShown();
  {
    base::RunLoop run_loop;
    scoped_refptr<ProcessMemoryMetricsEmitterFake> emitter(
        new ProcessMemoryMetricsEmitterFake(&run_loop,
                                            test_ukm_recorder_.get()));
    emitter->FetchAndEmitProcessMemoryMetrics();
    run_loop.Run();
  }

  CheckAllMemoryMetrics(histogram_tester, 2, kNumRenderers);
  CheckAllUkmEntries(2);
  CheckPageInfoUkmMetrics(url1, false /* is_visible */, 2);
  CheckPageInfoUkmMetrics(url2, true /* is_visible */, 2);
}

// Build id is only emitted for official builds.
#if defined(OFFICIAL_BUILD)
#define MAYBE_RendererBuildId RendererBuildId
#else
#define MAYBE_RendererBuildId DISABLED_RendererBuildId
#endif
IN_PROC_BROWSER_TEST_F(ProcessMemoryMetricsEmitterTest, MAYBE_RendererBuildId) {
  for (content::RenderProcessHost::iterator rph_iter =
           content::RenderProcessHost::AllHostsIterator();
       !rph_iter.IsAtEnd(); rph_iter.Advance()) {
    const base::Process& process = rph_iter.GetCurrentValue()->GetProcess();
    auto maps =
        memory_instrumentation::OSMetrics::GetProcessMemoryMaps(process.Pid());
    bool found = false;
    for (const memory_instrumentation::mojom::VmRegionPtr& region : maps) {
      if (region->module_debugid.empty())
        continue;
      found = true;
      break;
    }
    EXPECT_TRUE(found);
  }
}
