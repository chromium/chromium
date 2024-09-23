// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <atomic>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/queue.h"
#include "base/containers/stack.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/lock.h"
#include "base/task/post_job.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace base {

namespace {

// The perftest implements the following assignment strategy:
// - Naive: See RunJobWithNaiveAssignment().
// - Dynamic: See RunJobWithDynamicAssignment().
// - Loop around: See RunJobWithLoopAround().
// The following test setups exists for different strategies, although
// not every combination is performed:
// - No-op: Work items are no-op tasks.
// - No-op + disrupted: 10 disruptive tasks are posted every 1ms.
// - Busy wait: Work items are busy wait for 5us.
// - Busy wait + disrupted

constexpr char kMetricPrefixJob[] = "Job.";
constexpr char kMetricWorkThroughput[] = "work_throughput";
constexpr char kStoryNoOpNaive[] = "noop_naive";
constexpr char kStoryBusyWaitNaive[] = "busy_wait_naive";
constexpr char kStoryNoOpAtomic[] = "noop_atomic";
constexpr char kStoryNoOpAtomicDisrupted[] = "noop_atomic_disrupted";
constexpr char kStoryBusyWaitAtomic[] = "busy_wait_atomic";
constexpr char kStoryBusyWaitAtomicDisrupted[] = "busy_wait_atomic_disrupted";
constexpr char kStoryNoOpDynamic[] = "noop_dynamic";
constexpr char kStoryNoOpDynamicDisrupted[] = "noop_dynamic_disrupted";
constexpr char kStoryBusyWaitDynamic[] = "busy_wait_dynamic";
constexpr char kStoryBusyWaitDynamicDisrupted[] = "busy_wait_dynamic_disrupted";
constexpr char kStoryNoOpLoopAround[] = "noop_loop_around";
constexpr char kStoryNoOpLoopAroundDisrupted[] = "noop_loop_around_disrupted";
constexpr char kStoryBusyWaitLoopAround[] = "busy_wait_loop_around";
constexpr char kStoryBusyWaitLoopAroundDisrupted[] =
    "busy_wait_loop_around_disrupted";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixJob, story_name);
  reporter.RegisterImportantMetric(kMetricWorkThroughput, "tasks/ms");
  return reporter;
}

// A thread-safe data structure that generates heuristic starting points in a
// range to process items in parallel.
// Note: we could expose this atomic-binary-search-index-generator in
// //base/util if it's useful for real-world use cases.
class IndexGenerator {
 public:
  explicit IndexGenerator(size_t size) : size_(size) {
    AutoLock auto_lock(lock_);
    pending_indices_.push(0);
    ranges_to_split_.push({0, size_});
  }

  IndexGenerator(const IndexGenerator&) = delete;
  IndexGenerator& operator=(const IndexGenerator&) = delete;

  std::optional<size_t> GetNext() {
    AutoLock auto_lock(lock_);
    if (!pending_indices_.empty()) {
      // Return any pending index first.
      auto index = pending_indices_.top();
      pending_indices_.pop();
      return index;
    }
    if (ranges_to_split_.empty())
      return std::nullopt;

    // Split the oldest running range in 2 and return the middle index as
    // starting point.
    auto range = ranges_to_split_.front();
    ranges_to_split_.pop();
    size_t size = range.second - range.first;
    size_t mid = range.first + size / 2;
    // Both sides of the range are added to |ranges_to_split_| so they may be
    // further split if possible.
    if (mid - range.first > 1)
      ranges_to_split_.push({range.first, mid});
    if (range.second - mid > 1)
      ranges_to_split_.push({mid, range.second});
    return mid;
  }

  void GiveBack(size_t index) {
    AutoLock auto_lock(lock_);
    // Add |index| to pending indices so GetNext() may return it before anything
    // else.
    pending_indices_.push(index);
  }

 private:
  base::Lock lock_;
  // Pending indices that are ready to be handed out, prioritized over
  // |pending_ranges_| when non-empty.
  base::stack<size_t> pending_indices_ GUARDED_BY(lock_);
  // Pending [start, end] (exclusive) ranges to split and hand out indices from.
  base::queue<std::pair<size_t, size_t>> ranges_to_split_ GUARDED_BY(lock_);
  const size_t size_;
};

struct WorkItem {
  std::atomic_bool acquire{false};

  bool TryAcquire() {
    // memory_order_relaxed is sufficient as the WorkItem's state itself hasn't
    // been modified since the beginning of its associated job. This is only
    // atomically acquiring the right to work on it.
    return acquire.exchange(true, std::memory_order_relaxed) == false;
  }
};

class WorkList {
 public:
  WorkList(size_t num_work_items, RepeatingCallback<void(size_t)> process_item)
      : num_incomplete_items_(num_work_items),
        items_(num_work_items),
        process_item_(std::move(process_item)) {}

  WorkList(const WorkList&) = delete;
  WorkList& operator=(const WorkList&) = delete;

  // Acquires work item at |index|. Returns true if successful, or false if the
  // item was already acquired.
  bool TryAcquire(size_t index) { return items_[index].TryAcquire(); }

  // Processes work item at |index|. Returns true if there are more work items
  // to process, or false if all items were processed.
  bool ProcessWorkItem(size_t index) {
    process_item_.Run(index);
    return num_incomplete_items_.fetch_sub(1, std::memory_order_relaxed) > 1;
  }

  size_t NumIncompleteWorkItems(size_t /*worker_count*/) const {
    // memory_order_relaxed is sufficient since this is not synchronized with
    // other state.
    return num_incomplete_items_.load(std::memory_order_relaxed);
  }

  size_t NumWorkItems() const { return items_.size(); }

 private:
  std::atomic_size_t num_incomplete_items_;
  std::vector<WorkItem> items_;
  RepeatingCallback<void(size_t)> process_item_;
};

RepeatingCallback<void(size_t)> BusyWaitCallback(TimeDelta delta) {
  return base::BindRepeating(
      [](base::TimeDelta duration, size_t index) {
        const base::TimeTicks end_time = base::TimeTicks::Now() + duration;
        while (base::TimeTicks::Now() < end_time)
          ;
      },
      delta);
}

// Posts |task_count| no-op tasks every |delay|.
void DisruptivePostTasks(size_t task_count, TimeDelta delay) {
  for (size_t i = 0; i < task_count; ++i) {
    ThreadPool::PostTask(FROM_HERE, {TaskPriority::USER_BLOCKING}, DoNothing());
  }
  ThreadPool::PostDelayedTask(FROM_HERE, {TaskPriority::USER_BLOCKING},
                              BindOnce(&DisruptivePostTasks, task_count, delay),
                              delay);
}

class JobPerfTest : public testing::Test {
 public:
  JobPerfTest() = default;

  JobPerfTest(const JobPerfTest&) = delete;
  JobPerfTest& operator=(const JobPerfTest&) = delete;

  // Process |num_work_items| items with |process_item| in parallel. Work is
  // assigned by having each worker sequentially traversing all items and
  // acquiring unvisited ones.
  void RunJobWithNaiveAssignment(const std::string& story_name,
                                 size_t num_work_items,
                                 RepeatingCallback<void(size_t)> process_item) {
    WorkList work_list(num_work_items, std::move(process_item));

    const TimeTicks job_run_start = TimeTicks::Now();

    WaitableEvent complete;
    auto handle = PostJob(
        FROM_HERE, {TaskPriority::USER_VISIBLE},
        BindRepeating(
            [](WorkList* work_list, WaitableEvent* complete,
               JobDelegate* delegate) {
              for (size_t i = 0; i < work_list->NumWorkItems() &&
                                 work_list->NumIncompleteWorkItems(0) != 0 &&
                                 !delegate->ShouldYield();
                   ++i) {
                if (!work_list->TryAcquire(i))
                  continue;
                if (!work_list->ProcessWorkItem(i)) {
                  complete->Signal();
                  return;
                }
              }
            },
            Unretained(&work_list), Unretained(&complete)),
        BindRepeating(&WorkList::NumIncompleteWorkItems,
                      Unretained(&work_list)));

    complete.Wait();
    handle.Join();
    const TimeDelta job_duration = TimeTicks::Now() - job_run_start;
    EXPECT_EQ(0U, work_list.NumIncompleteWorkItems(0));

    auto reporter = SetUpReporter(story_name);
    reporter.AddResult(kMetricWorkThroughput,
                       size_t(num_work_items / job_duration.InMilliseconds()));
  }

  // Process |num_work_items| items with |process_item| in parallel. Work is
  // assigned by having each worker sequentially traversing all items
  // synchronized with an atomic variable.
  void RunJobWithAtomicAssignment(const std::string& story_name,
                                  size_t num_work_items,
                                  RepeatingCallback<void(size_t)> process_item,
                                  bool disruptive_post_tasks = false) {
    WorkList work_list(num_work_items, std::move(process_item));
    std::atomic_size_t index{0};

    // Post extra tasks to disrupt Job execution and cause workers to yield.
    if (disruptive_post_tasks)
      DisruptivePostTasks(10, Milliseconds(1));

    const TimeTicks job_run_start = TimeTicks::Now();

    WaitableEvent complete;
    auto handle = PostJob(
        FROM_HERE, {TaskPriority::USER_VISIBLE},
        BindRepeating(
            [](WorkList* work_list, WaitableEvent* complete,
               std::atomic_size_t* index, JobDelegate* delegate) {
              while (!delegate->ShouldYield()) {
                const size_t i = index->fetch_add(1, std::memory_order_relaxed);
                if (i >= work_list->NumWorkItems() ||
                    !work_list->ProcessWorkItem(i)) {
                  complete->Signal();
                  return;
                }
              }
            },
            Unretained(&work_list), Unretained(&complete), Unretained(&index)),
        BindRepeating(&WorkList::NumIncompleteWorkItems,
                      Unretained(&work_list)));

    complete.Wait();
    handle.Join();
    const TimeDelta job_duration = TimeTicks::Now() - job_run_start;
    EXPECT_EQ(0U, work_list.NumIncompleteWorkItems(0));

    auto reporter = SetUpReporter(story_name);
    reporter.AddResult(kMetricWorkThroughput,
                       size_t(num_work_items / job_duration.InMilliseconds()));
  }

  // Process |num_work_items| items with |process_item| in parallel. Work is
  // assigned dynamically having each new worker given a different point far
  // from other workers until all work is done. This is achieved by recursively
  // splitting each range that was previously given in half.
  void RunJobWithDynamicAssignment(const std::string& story_name,
                                   size_t num_work_items,
                                   RepeatingCallback<void(size_t)> process_item,
                                   bool disruptive_post_tasks = false) {
    WorkList work_list(num_work_items, std::move(process_item));
    IndexGenerator generator(num_work_items);

    // Post extra tasks to disrupt Job execution and cause workers to yield.
    if (disruptive_post_tasks)
      DisruptivePostTasks(10, Milliseconds(1));

    const TimeTicks job_run_start = TimeTicks::Now();

    WaitableEvent complete;
    auto handle = PostJob(
        FROM_HERE, {TaskPriority::USER_VISIBLE},
        BindRepeating(
            [](IndexGenerator* generator, WorkList* work_list,
               WaitableEvent* complete, JobDelegate* delegate) {
              while (work_list->NumIncompleteWorkItems(0) != 0 &&
                     !delegate->ShouldYield()) {
                std::optional<size_t> index = generator->GetNext();
                if (!index)
                  return;
                for (size_t i = *index; i < work_list->NumWorkItems(); ++i) {
                  if (delegate->ShouldYield()) {
                    generator->GiveBack(i);
                    return;
                  }
                  if (!work_list->TryAcquire(i)) {
                    // If this was touched already, get a new starting point.
                    break;
                  }
                  if (!work_list->ProcessWorkItem(i)) {
                    complete->Signal();
                    return;
                  }
                }
              }
            },
            Unretained(&generator), Unretained(&work_list),
            Unretained(&complete)),
        BindRepeating(&WorkList::NumIncompleteWorkItems,
                      Unretained(&work_list)));

    complete.Wait();
    handle.Join();
    const TimeDelta job_duration = TimeTicks::Now() - job_run_start;
    EXPECT_EQ(0U, work_list.NumIncompleteWorkItems(0));

    auto reporter = SetUpReporter(story_name);
    reporter.AddResult(kMetricWorkThroughput,
                       size_t(num_work_items / job_duration.InMilliseconds()));
  }

  // Process |num_work_items| items with |process_item| in parallel. Work is
  // assigned having each new worker given a different starting point far from
  // other workers and loop over all work items from there. This is achieved by
  // recursively splitting each range that was previously given in half.
  void RunJobWithLoopAround(const std::string& story_name,
                            size_t num_work_items,
                            RepeatingCallback<void(size_t)> process_item,
                            bool disruptive_post_tasks = false) {
    WorkList work_list(num_work_items, std::move(process_item));
    IndexGenerator generator(num_work_items);

    // Post extra tasks to disrupt Job execution and cause workers to yield.
    if (disruptive_post_tasks)
      DisruptivePostTasks(10, Milliseconds(1));

    const TimeTicks job_run_start = TimeTicks::Now();

    WaitableEvent complete;
    auto handle =
        PostJob(FROM_HERE, {TaskPriority::USER_VISIBLE},
                BindRepeating(
                    [](IndexGenerator* generator, WorkList* work_list,
                       WaitableEvent* complete, JobDelegate* delegate) {
                      std::optional<size_t> index = generator->GetNext();
                      if (!index)
                        return;
                      size_t i = *index;
                      while (true) {
                        if (delegate->ShouldYield()) {
                          generator->GiveBack(i);
                          return;
                        }
                        if (!work_list->TryAcquire(i)) {
                          // If this was touched already, skip.
                          continue;
                        }
                        if (!work_list->ProcessWorkItem(i)) {
                          // This will cause the loop to exit if there's no work
                          // left.
                          complete->Signal();
                          return;
                        }
                        ++i;
                        if (i == work_list->NumWorkItems())
                          i = 0;
                      }
                    },
                    Unretained(&generator), Unretained(&work_list),
                    Unretained(&complete)),
                BindRepeating(&WorkList::NumIncompleteWorkItems,
                              Unretained(&work_list)));

    complete.Wait();
    handle.Join();
    const TimeDelta job_duration = TimeTicks::Now() - job_run_start;
    EXPECT_EQ(0U, work_list.NumIncompleteWorkItems(0));

    auto reporter = SetUpReporter(story_name);
    reporter.AddResult(kMetricWorkThroughput,
                       size_t(num_work_items / job_duration.InMilliseconds()));
  }

 private:
  test::TaskEnvironment task_environment;
};

}  // namespace

TEST_F(JobPerfTest, NoOpWorkNaiveAssignment) {
  RunJobWithNaiveAssignment(kStoryNoOpNaive, 10000000, DoNothing());
}

TEST_F(JobPerfTest, BusyWaitNaiveAssignment) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithNaiveAssignment(kStoryBusyWaitNaive, 500000, std::move(callback));
}

TEST_F(JobPerfTest, NoOpWorkAtomicAssignment) {
  RunJobWithAtomicAssignment(kStoryNoOpAtomic, 10000000, DoNothing());
}

TEST_F(JobPerfTest, NoOpDisruptedWorkAtomicAssignment) {
  RunJobWithAtomicAssignment(kStoryNoOpAtomicDisrupted, 10000000, DoNothing(),
                             true);
}

TEST_F(JobPerfTest, BusyWaitAtomicAssignment) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithAtomicAssignment(kStoryBusyWaitAtomic, 500000, std::move(callback));
}

TEST_F(JobPerfTest, BusyWaitDisruptedWorkAtomicAssignment) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithAtomicAssignment(kStoryBusyWaitAtomicDisrupted, 500000,
                             std::move(callback), true);
}

TEST_F(JobPerfTest, NoOpWorkDynamicAssignment) {
  RunJobWithDynamicAssignment(kStoryNoOpDynamic, 10000000, DoNothing());
}

TEST_F(JobPerfTest, NoOpDisruptedWorkDynamicAssignment) {
  RunJobWithDynamicAssignment(kStoryNoOpDynamicDisrupted, 10000000, DoNothing(),
                              true);
}

TEST_F(JobPerfTest, BusyWaitWorkDynamicAssignment) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithDynamicAssignment(kStoryBusyWaitDynamic, 500000,
                              std::move(callback));
}

TEST_F(JobPerfTest, BusyWaitDisruptedWorkDynamicAssignment) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithDynamicAssignment(kStoryBusyWaitDynamicDisrupted, 500000,
                              std::move(callback), true);
}

TEST_F(JobPerfTest, NoOpWorkLoopAround) {
  RunJobWithLoopAround(kStoryNoOpLoopAround, 10000000, DoNothing());
}

TEST_F(JobPerfTest, NoOpDisruptedWorkLoopAround) {
  RunJobWithLoopAround(kStoryNoOpLoopAroundDisrupted, 10000000, DoNothing(),
                       true);
}

TEST_F(JobPerfTest, BusyWaitWorkLoopAround) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithLoopAround(kStoryBusyWaitLoopAround, 500000, std::move(callback));
}

TEST_F(JobPerfTest, BusyWaitDisruptedWorkLoopAround) {
  RepeatingCallback<void(size_t)> callback = BusyWaitCallback(Microseconds(5));
  RunJobWithLoopAround(kStoryBusyWaitLoopAroundDisrupted, 500000,
                       std::move(callback), true);
}

}  // namespace base
