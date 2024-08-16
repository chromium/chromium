// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/task_manager/sampling/shared_sampler.h"

#include <windows.h>

#include <winternl.h>

#include <algorithm>

#include "base/bit_cast.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/task_manager/sampling/shared_sampler_win_defines.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"

// ntstatus.h conflicts with windows.h so define this locally.
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

namespace task_manager {

static SharedSampler::QuerySystemInformationForTest
    g_query_system_information_for_test = nullptr;

// static
void SharedSampler::SetQuerySystemInformationForTest(
    QuerySystemInformationForTest query_system_information) {
  g_query_system_information_for_test = query_system_information;
}

namespace {

// Simple memory buffer wrapper for passing the data out of
// QuerySystemProcessInformation.
class ByteBuffer {
 public:
  explicit ByteBuffer(size_t capacity)
      : size_(0), capacity_(0) {
    if (capacity > 0)
      grow(capacity);
  }

  ByteBuffer(const ByteBuffer&) = delete;
  ByteBuffer& operator=(const ByteBuffer&) = delete;

  ~ByteBuffer() {}

  BYTE* data() { return data_.get(); }

  size_t size() { return size_; }

  void set_size(size_t new_size) {
    DCHECK_LE(new_size, capacity_);
    size_ = new_size;
  }

  size_t capacity() { return capacity_; }

  void grow(size_t new_capacity) {
    DCHECK_GT(new_capacity, capacity_);
    capacity_ = new_capacity;
    data_.reset(new BYTE[new_capacity]);
  }

 private:
  std::unique_ptr<BYTE[]> data_;
  size_t size_;
  size_t capacity_;
};

// Wrapper for NtQuerySystemProcessInformation with buffer reallocation logic.
bool QuerySystemProcessInformation(ByteBuffer* buffer) {
  HMODULE ntdll = ::GetModuleHandle(L"ntdll.dll");
  if (!ntdll) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  NTQUERYSYSTEMINFORMATION nt_query_system_information_ptr =
      reinterpret_cast<NTQUERYSYSTEMINFORMATION>(
          ::GetProcAddress(ntdll, "NtQuerySystemInformation"));
  if (!nt_query_system_information_ptr) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  NTSTATUS result;

  // There is a potential race condition between growing the buffer and new
  // processes being created. Try a few times before giving up.
  for (int i = 0; i < 10; i++) {
    ULONG data_size = 0;
    ULONG buffer_size = static_cast<ULONG>(buffer->capacity());

    if (g_query_system_information_for_test) {
      data_size =
          g_query_system_information_for_test(buffer->data(), buffer_size);
      result =
          (data_size > buffer_size) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    } else {
      result = nt_query_system_information_ptr(
          SystemProcessInformation, buffer->data(), buffer_size, &data_size);
    }

    if (result == STATUS_SUCCESS) {
      buffer->set_size(data_size);
      break;
    }

    if (result == STATUS_INFO_LENGTH_MISMATCH ||
        result == STATUS_BUFFER_TOO_SMALL) {
      // Insufficient buffer. Grow to the returned |data_size| plus 10% extra
      // to avoid frequent reallocations and try again.
      DCHECK_GT(data_size, buffer_size);
      buffer->grow(static_cast<ULONG>(data_size * 1.1));
    } else {
      // An error other than the two above.
      break;
    }
  }

  return result == STATUS_SUCCESS;
}

// Per-thread data extracted from SYSTEM_THREAD_INFORMATION and stored in a
// snapshot. This structure is accessed only on the worker thread.
struct ThreadData {
  base::PlatformThreadId thread_id;
  ULONG context_switches;
};

// Per-process data extracted from SYSTEM_PROCESS_INFORMATION and stored in a
// snapshot. This structure is accessed only on the worker thread.
struct ProcessData {
  ProcessData() = default;
  ProcessData(const ProcessData&) = delete;
  ProcessData& operator=(const ProcessData&) = delete;
  ProcessData(ProcessData&&) = default;

  int64_t hard_fault_count;
  base::Time start_time;
  base::TimeDelta cpu_time;
  std::vector<ThreadData> threads;
};

typedef std::map<base::ProcessId, ProcessData> ProcessDataMap;

ULONG CountContextSwitchesDelta(const ProcessData& prev_process_data,
  const ProcessData& new_process_data) {
  // This one pass algorithm relies on the threads vectors to be
  // ordered by thread_id.
  ULONG delta = 0;
  size_t prev_index = 0;

  for (const auto& new_thread : new_process_data.threads) {
    ULONG prev_thread_context_switches = 0;

    // Iterate over the process threads from the previous snapshot skipping
    // threads that don't exist anymore. Please note that this iteration starts
    // from the last known prev_index and goes until a previous snapshot's
    // thread ID >= the current snapshot's thread ID. So the overall algorithm
    // is linear.
    for (; prev_index < prev_process_data.threads.size(); ++prev_index) {
      const auto& prev_thread = prev_process_data.threads[prev_index];
      if (prev_thread.thread_id == new_thread.thread_id) {
        // Threads match between two snapshots. Use the previous snapshot
        // thread's context_switches to subtract from the delta.
        prev_thread_context_switches = prev_thread.context_switches;
        ++prev_index;
        break;
      }

      if (prev_thread.thread_id > new_thread.thread_id) {
        // This is due to a new thread that didn't exist in the previous
        // snapshot. Keep the zero value of |prev_thread_context_switches| which
        // essentially means the entire number of context switches of the new
        // thread will be added to the delta.
        break;
      }
    }

    delta += new_thread.context_switches - prev_thread_context_switches;
  }

  return delta;
}

// Seeks a matching ProcessData by Process ID in a previous snapshot.
// This uses the fact that ProcessDataMap entries are ordered by Process ID.
const ProcessData* SeekInPreviousSnapshot(
  base::ProcessId process_id, ProcessDataMap::const_iterator* iter_to_advance,
  const ProcessDataMap::const_iterator& range_end) {
  for (; *iter_to_advance != range_end; ++(*iter_to_advance)) {
    if ((*iter_to_advance)->first == process_id) {
      return &((*iter_to_advance)++)->second;
    }
    if ((*iter_to_advance)->first > process_id)
      break;
  }

  return nullptr;
}

// A wrapper function converting ticks (in units of 100 ns) to Time.
base::Time ConvertTicksToTime(uint64_t ticks) {
  FILETIME ft = base::bit_cast<FILETIME, uint64_t>(ticks);
  return base::Time::FromFileTime(ft);
}

// A wrapper function converting ticks (in units of 100 ns) to TimeDelta.
base::TimeDelta ConvertTicksToTimeDelta(uint64_t ticks) {
  return base::Microseconds(ticks / 10);
}

}  // namespace

// ProcessDataSnapshot gets created and accessed only on the worker thread.
// This is used to calculate metrics like Idle Wakeups / sec that require
// a delta between two snapshots.
// Please note that ProcessDataSnapshot has to be outside of anonymous namespace
// in order to match the declaration in shared_sampler.h.
struct ProcessDataSnapshot {
  ProcessDataMap processes;
  base::TimeTicks timestamp;
};

SharedSampler::SharedSampler(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner)
    : refresh_flags_(0), previous_buffer_size_(0),
      supported_image_names_(GetSupportedImageNames()),
      blocking_pool_runner_(blocking_pool_runner) {
  DCHECK(blocking_pool_runner.get());

  // This object will be created on the UI thread, however the sequenced checker
  // will be used to assert we're running the expensive operations on one of the
  // blocking pool threads.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(worker_pool_sequenced_checker_);
}

SharedSampler::~SharedSampler() {}

int64_t SharedSampler::GetSupportedFlags() const {
  return REFRESH_TYPE_IDLE_WAKEUPS | REFRESH_TYPE_START_TIME |
         REFRESH_TYPE_CPU_TIME | REFRESH_TYPE_HARD_FAULTS;
}

void SharedSampler::RegisterCallback(
    base::ProcessId process_id,
    OnSamplingCompleteCallback on_sampling_complete) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (process_id == 0)
    return;

  bool result =
      callbacks_map_.emplace(process_id, std::move(on_sampling_complete))
          .second;
  DCHECK(result);
}

void SharedSampler::UnregisterCallback(base::ProcessId process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (process_id == 0)
    return;

  callbacks_map_.erase(process_id);

  if (callbacks_map_.empty())
    ClearState();
}

void SharedSampler::Refresh(base::ProcessId process_id, int64_t refresh_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(0, refresh_flags & GetSupportedFlags());

  if (process_id == 0)
    return;

  DCHECK(callbacks_map_.find(process_id) != callbacks_map_.end());

  if (refresh_flags_ == 0) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&SharedSampler::RefreshOnWorkerThread, this),
        base::BindOnce(&SharedSampler::OnRefreshDone, this));
  } else {
    // http://crbug.com/678471
    // A group of consecutive Refresh calls should all specify the same refresh
    // flags. Rarely RefreshOnWorkerThread could take a long time (> 1 sec),
    // long enough for a next refresh cycle to start before results are ready
    // from a previous cycle. In that case refresh_flags_ would still remain
    // set to the previous cycle refresh flags which might be different than
    // this cycle refresh flags if a column was added or removed between the two
    // cycles. The worst that could happen in that condition is that results for
    // a newly added column would be missing for one extra refresh cycle.
  }

  refresh_flags_ |= refresh_flags;
}

void SharedSampler::ClearState() {
  previous_snapshot_.reset();
}

SharedSampler::AllSamplingResults SharedSampler::RefreshOnWorkerThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);

  AllSamplingResults results;

  std::unique_ptr<ProcessDataSnapshot> snapshot = CaptureSnapshot();
  if (snapshot) {
    if (previous_snapshot_) {
      results = MakeResultsFromTwoSnapshots(*previous_snapshot_, *snapshot);
    } else {
      results = MakeResultsFromSnapshot(*snapshot);
    }

    previous_snapshot_ = std::move(snapshot);
  } else {
    // Failed to get snapshot. This is unlikely.
    ClearState();
  }

  return results;
}

/* static */
std::vector<base::FilePath> SharedSampler::GetSupportedImageNames() {
  const wchar_t kNacl64Exe[] = L"nacl64.exe";

  std::vector<base::FilePath> supported_names;

  base::FilePath current_exe;
  if (base::PathService::Get(base::FILE_EXE, &current_exe))
    supported_names.push_back(current_exe.BaseName());

  supported_names.push_back(
      base::FilePath(chrome::kBrowserProcessExecutableName));
  supported_names.push_back(base::FilePath(kNacl64Exe));

  return supported_names;
}

bool SharedSampler::IsSupportedImageName(
    base::FilePath::StringPieceType image_name) const {
  for (const base::FilePath& supported_name : supported_image_names_) {
    if (base::FilePath::CompareEqualIgnoreCase(image_name,
                                               supported_name.value()))
      return true;
  }

  return false;
}

std::unique_ptr<ProcessDataSnapshot> SharedSampler::CaptureSnapshot() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);

  // Preallocate the buffer with the size determined on the previous call to
  // QuerySystemProcessInformation. This should be sufficient most of the time.
  // QuerySystemProcessInformation will grow the buffer if necessary.
  ByteBuffer data_buffer(previous_buffer_size_);

  if (!QuerySystemProcessInformation(&data_buffer))
    return nullptr;

  previous_buffer_size_ = data_buffer.capacity();

  std::unique_ptr<ProcessDataSnapshot> snapshot(new ProcessDataSnapshot);
  snapshot->timestamp = base::TimeTicks::Now();

  for (size_t offset = 0; offset < data_buffer.size(); ) {
    const auto* pi = reinterpret_cast<const SYSTEM_PROCESS_INFORMATION*>(
        data_buffer.data() + offset);

    // Validate that the offset is valid and all needed data is within
    // the buffer boundary.
    if (offset + sizeof(SYSTEM_PROCESS_INFORMATION) > data_buffer.size())
      break;
    if (pi->NumberOfThreads > 0 &&
        (offset + sizeof(SYSTEM_PROCESS_INFORMATION) +
             (pi->NumberOfThreads - 1) * sizeof(SYSTEM_THREAD_INFORMATION) >
         data_buffer.size())) {
      break;
    }

    if (pi->ImageName.Buffer) {
      // Validate that the image name is within the buffer boundary.
      // ImageName.Length seems to be in bytes rather than characters.
      ULONG image_name_offset =
          reinterpret_cast<BYTE*>(pi->ImageName.Buffer) - data_buffer.data();
      if (image_name_offset + pi->ImageName.Length > data_buffer.size())
        break;

      // Check if this is a chrome process. Ignore all other processes.
      if (IsSupportedImageName(pi->ImageName.Buffer)) {
        // Collect enough data to be able to do a diff between two snapshots.
        // Some threads might stop or new threads might be created between two
        // snapshots. If a thread with a large number of context switches gets
        // terminated the total number of context switches for the process might
        // go down and the delta would be negative.
        // To avoid that we need to compare thread IDs between two snapshots and
        // not count context switches for threads that are missing in the most
        // recent snapshot.
        ProcessData process_data;
        process_data.hard_fault_count = pi->HardFaultCount;
        process_data.start_time = ConvertTicksToTime(pi->CreateTime);
        process_data.cpu_time =
            ConvertTicksToTimeDelta(pi->KernelTime + pi->UserTime);

        // Iterate over threads and store each thread's ID and number of context
        // switches.
        for (ULONG thread_index = 0; thread_index < pi->NumberOfThreads;
             ++thread_index) {
          const SYSTEM_THREAD_INFORMATION* ti = &pi->Threads[thread_index];
          if (ti->ClientId.UniqueProcess != pi->ProcessId)
            continue;

          ThreadData thread_data;
          thread_data.thread_id = static_cast<base::PlatformThreadId>(
              reinterpret_cast<uintptr_t>(ti->ClientId.UniqueThread));
          thread_data.context_switches = ti->ContextSwitchCount;
          process_data.threads.push_back(thread_data);
        }

        // Order thread data by thread ID to help diff two snapshots.
        std::sort(process_data.threads.begin(), process_data.threads.end(),
            [](const ThreadData& l, const ThreadData r) {
              return l.thread_id < r.thread_id;
            });

        base::ProcessId process_id = static_cast<base::ProcessId>(
            reinterpret_cast<uintptr_t>(pi->ProcessId));
        bool inserted = snapshot->processes.insert(
            std::make_pair(process_id, std::move(process_data))).second;
        DCHECK(inserted);
      }
    }

    // Check for end of the list.
    if (!pi->NextEntryOffset)
      break;

    // Jump to the next entry.
    offset += pi->NextEntryOffset;
  }

  return snapshot;
}

SharedSampler::AllSamplingResults SharedSampler::MakeResultsFromTwoSnapshots(
    const ProcessDataSnapshot& prev_snapshot,
    const ProcessDataSnapshot& snapshot) {
  // Time delta in seconds.
  double time_delta = (snapshot.timestamp - prev_snapshot.timestamp)
      .InSecondsF();

  // Iterate over processes in both snapshots in parallel. This algorithm relies
  // on map entries being ordered by Process ID.
  ProcessDataMap::const_iterator prev_iter = prev_snapshot.processes.begin();

  AllSamplingResults results;
  results.reserve(snapshot.processes.size());
  for (const auto& current_entry : snapshot.processes) {
    base::ProcessId process_id = current_entry.first;
    const ProcessData& process = current_entry.second;

    const ProcessData* prev_snapshot_process = SeekInPreviousSnapshot(
        process_id, &prev_iter, prev_snapshot.processes.end());

    // Delta between the old snapshot and the new snapshot.
    int64_t hard_faults_delta = 0;
    int idle_wakeups_delta;

    if (prev_snapshot_process) {
      hard_faults_delta =
          process.hard_fault_count - prev_snapshot_process->hard_fault_count;
      // Processes match between two snapshots. Diff context switches.
      idle_wakeups_delta =
          CountContextSwitchesDelta(*prev_snapshot_process, process);
    } else {
      // Process is missing in the previous snapshot.
      // Use entire number of context switches of the current process.
      idle_wakeups_delta = CountContextSwitchesDelta(ProcessData(), process);
    }

    ProcessIdAndSamplingResult result;
    result.process_id = process_id;
    result.data.hard_faults_per_second =
        static_cast<int>(round(hard_faults_delta / time_delta));
    result.data.idle_wakeups_per_second =
        static_cast<int>(round(idle_wakeups_delta / time_delta));
    result.data.start_time = process.start_time;
    result.data.cpu_time = process.cpu_time;
    results.push_back(result);
  }

  return results;
}

SharedSampler::AllSamplingResults SharedSampler::MakeResultsFromSnapshot(
    const ProcessDataSnapshot& snapshot) {
  AllSamplingResults results;
  results.reserve(snapshot.processes.size());
  for (const auto& pair : snapshot.processes) {
    ProcessIdAndSamplingResult result;
    result.process_id = pair.first;
    // Use 0 for Idle Wakeups / sec in this case. This is consistent with
    // ProcessMetrics::CalculateIdleWakeupsPerSecond implementation.
    result.data.hard_faults_per_second = 0;
    result.data.idle_wakeups_per_second = 0;
    result.data.start_time = pair.second.start_time;
    result.data.cpu_time = pair.second.cpu_time;
    results.push_back(result);
  }
  return results;
}

void SharedSampler::OnRefreshDone(AllSamplingResults refresh_results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(0, refresh_flags_);

  size_t result_index = 0;

  for (const auto& callback_entry : callbacks_map_) {
    base::ProcessId process_id = callback_entry.first;
    SamplingResult process_result;

    // Match refresh result by |process_id|.
    // This relies on refresh results being ordered by Process ID.
    // Please note that |refresh_results| might contain some extra entries that
    // don't exist in |callbacks_map_| if there is more than one instance of
    // Chrome. It might be missing some entries too if there is a race condition
    // between getting process information on the worker thread and adding a
    // corresponding TaskGroup to the task manager.
    for (; result_index < refresh_results.size(); ++result_index) {
      const auto& result = refresh_results[result_index];
      if (result.process_id == process_id) {
        // Data matched in |refresh_results|.
        process_result = std::move(result.data);
        ++result_index;
        break;
      }

      if (result.process_id > process_id) {
        // An entry corresponding to |process_id| is missing. See above.
        break;
      }
    }

    callback_entry.second.Run(std::move(process_result));
  }

  // Reset refresh_results_ to trigger RefreshOnWorkerThread next time Refresh
  // is called.
  refresh_flags_ = 0;
}

}  // namespace task_manager
