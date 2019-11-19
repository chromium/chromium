// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "components/heap_profiling/supervisor.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/zlib/zlib.h"

#if defined(OS_WIN)
#include <io.h>
#endif

namespace heap_profiling {

namespace {
// Slow-report trigger configuration.
// see: content/browser/tracing/background_tracing_config_impl.cc
const char kConfigsKey[] = "configs";
const char kConfigModeKey[] = "mode";
const char kConfigScenarioName[] = "scenario_name";
const char kConfigCategoryKey[] = "category";
const char kConfigCategoryMemlog[] = "MEMLOG";
const char kOOPHeapProfilingUploadUrl[] = "upload_url";

void OnTraceUploadComplete(TraceCrashServiceUploader* uploader,
                           bool success,
                           const std::string& feedback) {
  UMA_HISTOGRAM_BOOLEAN("HeapProfiling.UploadTrace.Success", success);

  if (!success) {
    LOG(ERROR) << "Cannot upload trace file: " << feedback;
    return;
  }

  // The reports is successfully sent. Reports the crash-id to ease debugging.
  LOG(WARNING) << "slow-reports sent: '" << feedback << '"';
}

void UploadTraceToCrashServer(std::string upload_url,
                              std::string file_contents,
                              std::string trigger_name,
                              uint32_t sampling_rate) {
  // Traces has been observed as small as 4k. Seems likely to be a bug. To
  // account for all potentially too-small traces, we set the lower bounds to
  // 512 bytes. The upper bounds is set to 300MB as an extra-high threshold,
  // just in case something goes wrong.
  UMA_HISTOGRAM_CUSTOM_COUNTS("HeapProfiling.UploadTrace.Size",
                              file_contents.size(), 512, 300 * 1024 * 1024, 50);

  base::Value rules_list(base::Value::Type::LIST);
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetKey("rule", base::Value("MEMLOG"));
  rule.SetKey("trigger_name", base::Value(std::move(trigger_name)));
  rules_list.Append(std::move(rule));

  std::string sampling_mode = base::StringPrintf("SAMPLING_%u", sampling_rate);

  base::Value configs(base::Value::Type::DICTIONARY);
  configs.SetKey(kConfigModeKey, base::Value(sampling_mode));
  configs.SetKey(kConfigCategoryKey, base::Value(kConfigCategoryMemlog));
  configs.SetKey(kConfigsKey, std::move(rules_list));

  std::unique_ptr<base::DictionaryValue> metadata =
      std::make_unique<base::DictionaryValue>();
  metadata->SetKey("config", std::move(configs));
  metadata->SetKey(kConfigScenarioName, base::Value("MEMLOG"));

  TraceCrashServiceUploader* uploader = new TraceCrashServiceUploader(
      g_browser_process->shared_url_loader_factory());
  if (!upload_url.empty())
    uploader->SetUploadURL(upload_url);

  uploader->DoUpload(file_contents, content::TraceUploader::COMPRESSED_UPLOAD,
                     std::move(metadata),
                     content::TraceUploader::UploadProgressCallback(),
                     base::Bind(&OnTraceUploadComplete, base::Owned(uploader)));
}

}  // namespace

ProfilingProcessHost::ProfilingProcessHost() : background_triggers_(this) {
  const std::string upload_url = base::GetFieldTrialParamValueByFeature(
      kOOPHeapProfilingFeature, kOOPHeapProfilingUploadUrl);
  if (GURL(upload_url).is_valid())
    upload_url_ = upload_url;
}

ProfilingProcessHost::~ProfilingProcessHost() = default;

void ProfilingProcessHost::Start() {
  // Only enable automatic uploads when the Finch experiment is enabled.
  // Developers can still manually upload via chrome://memory-internals.
  if (IsBackgroundHeapProfilingEnabled())
    background_triggers_.StartTimer();
  metrics_timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(24),
      base::Bind(&ProfilingProcessHost::ReportMetrics, base::Unretained(this)));
}

// static
ProfilingProcessHost* ProfilingProcessHost::GetInstance() {
  return base::Singleton<
      ProfilingProcessHost,
      base::LeakySingletonTraits<ProfilingProcessHost>>::get();
}

void ProfilingProcessHost::SaveTraceWithHeapDumpToFile(
    base::FilePath dest,
    SaveTraceFinishedCallback done,
    bool stop_immediately_after_heap_dump_for_tests) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  auto finish_trace_callback = base::BindOnce(
      [](base::FilePath dest, SaveTraceFinishedCallback done, bool success,
         std::string trace) {
        if (!success) {
          base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
              ->PostTask(FROM_HERE, base::BindOnce(std::move(done), false));
          return;
        }
        base::PostTask(
            FROM_HERE,
            {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
             base::MayBlock()},
            base::BindOnce(
                &ProfilingProcessHost::SaveTraceToFileOnBlockingThread,
                base::Unretained(ProfilingProcessHost::GetInstance()),
                std::move(dest), std::move(trace), std::move(done)));
      },
      std::move(dest), std::move(done));
  Supervisor::GetInstance()->RequestTraceWithHeapDump(
      std::move(finish_trace_callback), /*anonymize=*/false);
}

void ProfilingProcessHost::RequestProcessReport(std::string trigger_name) {
  // https://crbug.com/753218: Add e2e tests for this code path.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // It's safe to pass a raw pointer for ProfilingProcessHost because it's a
  // singleton that's never destroyed.
  auto finish_report_callback = base::BindOnce(
      [](std::string upload_url, std::string trigger_name,
         uint32_t sampling_rate, bool success, std::string trace) {
        UMA_HISTOGRAM_BOOLEAN("HeapProfiling.RecordTrace.Success", success);
        if (success) {
          UploadTraceToCrashServer(std::move(upload_url), std::move(trace),
                                   std::move(trigger_name), sampling_rate);
        }
      },
      upload_url_, std::move(trigger_name),
      Supervisor::GetInstance()->GetSamplingRate());
  Supervisor::GetInstance()->RequestTraceWithHeapDump(
      std::move(finish_report_callback), true /* anonymize */);
}

void ProfilingProcessHost::SaveTraceToFileOnBlockingThread(
    base::FilePath dest,
    std::string trace,
    SaveTraceFinishedCallback done) {
  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  // Pass ownership of the underlying fd/HANDLE to zlib.
  base::PlatformFile platform_file = file.TakePlatformFile();
#if defined(OS_WIN)
  // The underlying handle |platform_file| is also closed when |fd| is closed.
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(platform_file), 0);
#else
  int fd = platform_file;
#endif
  gzFile gz_file = gzdopen(fd, "w");
  if (!gz_file) {
    DLOG(ERROR) << "Cannot compress trace file";
    base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
        ->PostTask(FROM_HERE, base::BindOnce(std::move(done), false));
    return;
  }

  size_t written_bytes = gzwrite(gz_file, trace.c_str(), trace.size());
  gzclose(gz_file);

  base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
      ->PostTask(FROM_HERE, base::BindOnce(std::move(done),
                                           written_bytes == trace.size()));
}

void ProfilingProcessHost::ReportMetrics() {
  UMA_HISTOGRAM_ENUMERATION("HeapProfiling.ProfilingMode",
                            Supervisor::GetInstance()->GetMode(), Mode::kCount);
}

}  // namespace heap_profiling
