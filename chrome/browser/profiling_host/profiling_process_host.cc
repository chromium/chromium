// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/services/heap_profiling/public/cpp/controller.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/zlib/zlib.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <io.h>
#endif

namespace heap_profiling {

ProfilingProcessHost::ProfilingProcessHost() = default;
ProfilingProcessHost::~ProfilingProcessHost() = default;

void ProfilingProcessHost::Start() {
  metrics_timer_.Start(FROM_HERE, base::Hours(24),
                       base::BindRepeating(&ProfilingProcessHost::ReportMetrics,
                                           base::Unretained(this)));
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
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE, base::BindOnce(std::move(done), false));
          return;
        }
        base::ThreadPool::PostTask(
            FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
            base::BindOnce(
                &ProfilingProcessHost::SaveTraceToFileOnBlockingThread,
                base::Unretained(ProfilingProcessHost::GetInstance()),
                std::move(dest), std::move(trace), std::move(done)));
      },
      std::move(dest), std::move(done));
  Supervisor::GetInstance()->RequestTraceWithHeapDump(
      std::move(finish_trace_callback), /*anonymize=*/false);
}

void ProfilingProcessHost::SaveTraceToFileOnBlockingThread(
    base::FilePath dest,
    std::string trace,
    SaveTraceFinishedCallback done) {
  base::File file(dest,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  // Pass ownership of the underlying fd/HANDLE to zlib.
  base::PlatformFile platform_file = file.TakePlatformFile();
#if BUILDFLAG(IS_WIN)
  // The underlying handle |platform_file| is also closed when |fd| is closed.
  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(platform_file), 0);
#else
  int fd = platform_file;
#endif
  gzFile gz_file = gzdopen(fd, "w");
  if (!gz_file) {
    DLOG(ERROR) << "Cannot compress trace file";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(done), false));
    return;
  }

  size_t written_bytes = gzwrite(gz_file, trace.c_str(), trace.size());
  gzclose(gz_file);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(done), written_bytes == trace.size()));
}

void ProfilingProcessHost::ReportMetrics() {
  UMA_HISTOGRAM_ENUMERATION("HeapProfiling.ProfilingMode",
                            Supervisor::GetInstance()->GetMode(), Mode::kCount);
}

}  // namespace heap_profiling
