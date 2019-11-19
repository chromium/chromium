// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/heap_collector.h"

#include <inttypes.h>

#include <memory>
#include <string>
#include <utility>

#include "base/allocator/allocator_extension.h"
#include "base/command_line.h"
#include "base/debug/proc_maps_linux.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

namespace {

// Supported collection mode values.
const char kCollectionModeTcmalloc[] = "cwp-tcmalloc";
const char kCollectionModeShimLayer[] = "cwp-shim-layer";

// Name of the heap collector. It is appended to the UMA metric names for
// reporting collection and upload status.
const char kHeapCollectorName[] = "Heap";

// The approximate gap in bytes between sampling actions. Heap allocations are
// sampled using a geometric distribution with the specified mean.
const size_t kHeapSamplingIntervalBytes = 1 * 1024 * 1024;

// Feature parameters that control the behavior of the heap collector.
constexpr base::FeatureParam<int> kSamplingIntervalBytes{
    &heap_profiling::kOOPHeapProfilingFeature, "SamplingIntervalBytes",
    kHeapSamplingIntervalBytes};

constexpr base::FeatureParam<int> kPeriodicCollectionIntervalMs{
    &heap_profiling::kOOPHeapProfilingFeature, "PeriodicCollectionIntervalMs",
    3 * 3600 * 1000};  // 3h

constexpr base::FeatureParam<int> kResumeFromSuspendSamplingFactor{
    &heap_profiling::kOOPHeapProfilingFeature,
    "ResumeFromSuspend::SamplingFactor", 10};

constexpr base::FeatureParam<int> kResumeFromSuspendMaxDelaySec{
    &heap_profiling::kOOPHeapProfilingFeature, "ResumeFromSuspend::MaxDelaySec",
    5};

constexpr base::FeatureParam<int> kRestoreSessionSamplingFactor{
    &heap_profiling::kOOPHeapProfilingFeature, "RestoreSession::SamplingFactor",
    10};

constexpr base::FeatureParam<int> kRestoreSessionMaxDelaySec{
    &heap_profiling::kOOPHeapProfilingFeature, "RestoreSession::MaxDelaySec",
    10};

// Limit the total size of protobufs that can be cached, so they don't take up
// too much memory. If the size of cached protobufs exceeds this value, stop
// collecting further perf data. The current value is 2 MB.
const size_t kCachedHeapDataProtobufSizeThreshold = 2 * 1024 * 1024;

// Location of quipper on ChromeOS.
const char kQuipperLocation[] = "/usr/bin/quipper";

// Quipper arguments for passing in a profile and the process PID.
const char kQuipperHeapProfile[] = "input_heap_profile";
const char kQuipperProcessPid[] = "pid";

void DeleteFileAsync(const base::FilePath& path) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), std::move(path),
                     false));
}

// Deletes the temp file when the object goes out of scope.
class FileDeleter {
 public:
  explicit FileDeleter(const base::FilePath& path) : path_(path) {}
  ~FileDeleter() { DeleteFileAsync(path_); }

 private:
  const base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(FileDeleter);
};

void SetHeapSamplingPeriod(size_t sampling_period, HeapCollectionMode mode) {
  switch (mode) {
    case HeapCollectionMode::kNone:
      break;
    case HeapCollectionMode::kTcmalloc: {
      bool res = base::allocator::SetNumericProperty(
          "tcmalloc.sampling_period_bytes", sampling_period);
      DCHECK(res);
      break;
    }
    case HeapCollectionMode::kShimLayer: {
      base::SamplingHeapProfiler::Get()->SetSamplingInterval(sampling_period);
      break;
    }
  }
}

std::string CountAndSizeToString(uintptr_t count, uintptr_t size) {
  return base::StringPrintf("%" PRIuPTR ": %" PRIuPTR " [%" PRIuPTR
                            ": %" PRIuPTR "] @",
                            count, size, count, size);
}

void WriteProfileHeader(
    base::File* out,
    base::StringPiece label,
    const std::vector<base::SamplingHeapProfiler::Sample>& samples) {
  // Compute the total count and total size
  uintptr_t total_count = samples.size();
  uintptr_t total_size = 0;
  for (const auto& sample : samples) {
    total_size += sample.total;
  }

  std::string header = base::StrCat(
      {"heap profile: ", CountAndSizeToString(total_count, total_size), " ",
       label, "\n"});
  int res = out->WriteAtCurrentPos(header.c_str(), header.length());
  DCHECK_EQ(res, static_cast<int>(header.length()));
}

// Prints the process runtime mappings. Returns if the operation was a success.
bool PrintProcSelfMaps(base::File* out, const std::string& proc_maps) {
  std::vector<base::debug::MappedMemoryRegion> regions;
  if (!base::debug::ParseProcMaps(proc_maps, &regions))
    return false;

  int res = out->WriteAtCurrentPos("\nMAPPED_LIBRARIES:\n", 19);
  DCHECK_EQ(res, 19);

  for (const auto& region : regions) {
    // We assume 'flags' looks like 'rwxp' or 'rwx'.
    char r = (region.permissions & base::debug::MappedMemoryRegion::READ) ? 'r'
                                                                          : '-';
    char w = (region.permissions & base::debug::MappedMemoryRegion::WRITE)
                 ? 'w'
                 : '-';
    char x = (region.permissions & base::debug::MappedMemoryRegion::EXECUTE)
                 ? 'x'
                 : '-';
    char p = (region.permissions & base::debug::MappedMemoryRegion::PRIVATE)
                 ? 'p'
                 : '-';

    // The devices major / minor values and the inode are not filled by
    // ParseProcMaps, so write them as zero values. They are not relevant for
    // symbolization.
    std::string row = base::StringPrintf("%08" PRIxPTR "-%08" PRIxPTR
                                         " %c%c%c%c %08llx 00:00 0 %s\n",
                                         region.start, region.end, r, w, x, p,
                                         region.offset, region.path.c_str());

    int res = out->WriteAtCurrentPos(row.c_str(), row.length());
    DCHECK_EQ(res, static_cast<int>(row.length()));
  }
  return true;
}

// Fetches profile from shim layer sampler and attempts to write it to the given
// output file in the format used by the tcmalloc based heap sampler, with a
// header line, followed by a row for each sample, and a section with the
// process runtime mappings.
bool FetchShimProfileAndSaveToFile(base::File* out) {
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  std::string proc_maps;
  if (!base::debug::ReadProcMaps(&proc_maps))
    return false;

  return internal::WriteHeapProfileToFile(out, samples, proc_maps);
}

bool FetchProfileAndSaveToFile(base::File* out, HeapCollectionMode mode) {
  switch (mode) {
    case HeapCollectionMode::kNone:
      DCHECK(false) << "Collection attempted for collection mode NONE";
      return true;
    case HeapCollectionMode::kTcmalloc: {
      std::string writer;
      base::allocator::GetHeapSample(&writer);
      int res = out->WriteAtCurrentPos(writer.c_str(), writer.length());
      DCHECK_EQ(res, static_cast<int>(writer.length()));
      return true;
    }
    case HeapCollectionMode::kShimLayer:
      return FetchShimProfileAndSaveToFile(out);
  }
}

}  // namespace

namespace internal {

bool WriteHeapProfileToFile(
    base::File* out,
    const std::vector<base::SamplingHeapProfiler::Sample>& samples,
    const std::string& proc_maps) {
  WriteProfileHeader(out, "heap_v2/1", samples);
  for (const auto& sample : samples) {
    std::string row = CountAndSizeToString(1, sample.total);
    for (const void* frame : sample.stack) {
      base::StringAppendF(&row, " %p", frame);
    }
    row.append("\n", 1);
    int res = out->WriteAtCurrentPos(row.c_str(), row.length());
    DCHECK_EQ(res, static_cast<int>(row.length()));
  }
  return PrintProcSelfMaps(out, proc_maps);
}

}  // namespace internal

// static
HeapCollectionMode HeapCollector::CollectionModeFromString(std::string mode) {
  if (mode == kCollectionModeTcmalloc)
    return HeapCollectionMode::kTcmalloc;
  if (mode == kCollectionModeShimLayer)
    return HeapCollectionMode::kShimLayer;
  return HeapCollectionMode::kNone;
}

HeapCollector::HeapCollector(HeapCollectionMode mode)
    : internal::MetricCollector(kHeapCollectorName, CollectionParams()),
      mode_(mode),
      is_enabled_(false),
      sampling_period_bytes_(kHeapSamplingIntervalBytes) {
  if (mode_ == HeapCollectionMode::kShimLayer) {
    base::SamplingHeapProfiler::Init();
  }
}

const char* HeapCollector::ToolName() const {
  return kHeapCollectorName;
}

HeapCollector::~HeapCollector() {
  // Disable heap sampling when the collector exits.
  DisableSampling();
}

void HeapCollector::EnableSampling() {
  if (is_enabled_)
    return;
  switch (mode_) {
    case HeapCollectionMode::kNone:
      break;
    case HeapCollectionMode::kTcmalloc:
      SetHeapSamplingPeriod(sampling_period_bytes_, mode_);
      break;
    case HeapCollectionMode::kShimLayer:
      base::SamplingHeapProfiler::Get()->Start();
      break;
  }
  is_enabled_ = true;
}

void HeapCollector::DisableSampling() {
  if (!is_enabled_)
    return;
  switch (mode_) {
    case HeapCollectionMode::kNone:
      break;
    case HeapCollectionMode::kTcmalloc:
      SetHeapSamplingPeriod(0, mode_);
      break;
    case HeapCollectionMode::kShimLayer:
      base::SamplingHeapProfiler::Get()->Stop();
      break;
  }
  is_enabled_ = false;
}

void HeapCollector::SetUp() {
  if (base::FeatureList::IsEnabled(heap_profiling::kOOPHeapProfilingFeature)) {
    sampling_period_bytes_ = kSamplingIntervalBytes.Get();
    SetCollectionParamsFromFeatureParams();
  }

  // For the tcmalloc collector, we set the sampling period every time we enable
  // it. The shim layer sampler has a separate API for starting and stopping, so
  // we must set its sampling period once explicitly.
  if (mode_ == HeapCollectionMode::kShimLayer) {
    SetHeapSamplingPeriod(sampling_period_bytes_, mode_);
  }
  EnableSampling();
}

void HeapCollector::SetCollectionParamsFromFeatureParams() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CollectionParams& params = collection_params();
  params.periodic_interval =
      base::TimeDelta::FromMilliseconds(kPeriodicCollectionIntervalMs.Get());
  params.resume_from_suspend.sampling_factor =
      kResumeFromSuspendSamplingFactor.Get();
  params.resume_from_suspend.max_collection_delay =
      base::TimeDelta::FromSeconds(kResumeFromSuspendMaxDelaySec.Get());
  params.restore_session.sampling_factor = kRestoreSessionSamplingFactor.Get();
  params.restore_session.max_collection_delay =
      base::TimeDelta::FromSeconds(kRestoreSessionMaxDelaySec.Get());
}

base::WeakPtr<internal::MetricCollector> HeapCollector::GetWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

bool HeapCollector::ShouldCollect() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Do not collect further data if we've already collected a substantial amount
  // of data, as indicated by |kCachedHeapDataProtobufSizeThreshold|.
  if (cached_data_size_ >= kCachedHeapDataProtobufSizeThreshold) {
    AddToUmaHistogram(CollectionAttemptStatus::NOT_READY_TO_COLLECT);
    return false;
  }
  return true;
}

void HeapCollector::CollectProfile(
    std::unique_ptr<SampledProfile> sampled_profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (mode_ == HeapCollectionMode::kNone)
    return;

  auto incognito_observer = WindowedIncognitoMonitor::CreateObserver();
  // For privacy reasons, Chrome should only collect heap profiles if there is
  // no incognito session active (or gets spawned while the profile is saved).
  if (incognito_observer->IncognitoActive()) {
    AddToUmaHistogram(CollectionAttemptStatus::INCOGNITO_ACTIVE);
    return;
  }

  base::Optional<base::FilePath> temp_file =
      DumpProfileToTempFile(std::move(incognito_observer));
  if (!temp_file)
    return;

  auto quipper = MakeQuipperCommand(temp_file.value());
  ParseAndSaveProfile(std::move(quipper), std::move(temp_file.value()),
                      std::move(sampled_profile));
}

base::Optional<base::FilePath> HeapCollector::DumpProfileToTempFile(
    std::unique_ptr<WindowedIncognitoObserver> incognito_observer) {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    AddToUmaHistogram(CollectionAttemptStatus::UNABLE_TO_COLLECT);
    return base::nullopt;
  }
  base::File temp(temp_path, base::File::FLAG_CREATE_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  DCHECK(temp.created());
  DCHECK(temp.IsValid());

  bool success = FetchProfileAndSaveToFile(&temp, mode_);
  temp.Close();

  if (!success) {
    AddToUmaHistogram(CollectionAttemptStatus::DATA_COLLECTION_FAILED);
    DeleteFileAsync(temp_path);
    return base::nullopt;
  }
  if (incognito_observer->IncognitoLaunched()) {
    AddToUmaHistogram(CollectionAttemptStatus::INCOGNITO_LAUNCHED);
    DeleteFileAsync(temp_path);
    return base::nullopt;
  }
  return base::make_optional<base::FilePath>(temp_path);
}

// static
std::unique_ptr<base::CommandLine> HeapCollector::MakeQuipperCommand(
    const base::FilePath& profile_path) {
  auto quipper =
      std::make_unique<base::CommandLine>(base::FilePath(kQuipperLocation));
  quipper->AppendSwitchPath(kQuipperHeapProfile, profile_path);
  quipper->AppendSwitchASCII(kQuipperProcessPid,
                             base::NumberToString(base::GetCurrentProcId()));
  return quipper;
}

void HeapCollector::ParseAndSaveProfile(
    std::unique_ptr<base::CommandLine> parser,
    base::FilePath profile_path,
    std::unique_ptr<SampledProfile> sampled_profile) {
  // Parsing processor information may be expensive. Compute asynchronously
  // in a separate thread.
  auto task_runner = base::SequencedTaskRunnerHandle::Get();
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&HeapCollector::ParseProfileOnThreadPool, task_runner,
                     weak_factory_.GetWeakPtr(), std::move(parser),
                     std::move(profile_path), std::move(sampled_profile)));
}

// static
void HeapCollector::ParseProfileOnThreadPool(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::WeakPtr<HeapCollector> heap_collector,
    std::unique_ptr<base::CommandLine> parser,
    base::FilePath profile_path,
    std::unique_ptr<SampledProfile> sampled_profile) {
  // We may exit due to parsing errors, so use a FileDeleter to remove the
  // temporary profile data on all paths.
  FileDeleter file_deleter(profile_path);

  // Run the parser command on the profile file.
  std::string output;
  if (!base::GetAppOutput(*parser, &output)) {
    heap_collector->AddToUmaHistogram(
        CollectionAttemptStatus::ILLEGAL_DATA_RETURNED);
    return;
  }
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HeapCollector::SaveSerializedPerfProto, heap_collector,
                     std::move(sampled_profile), std::move(output)));
}

}  // namespace metrics
