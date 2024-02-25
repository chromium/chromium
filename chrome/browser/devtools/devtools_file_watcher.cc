// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_watcher.h"

#include <map>
#include <memory>
#include <set>
#include <unordered_map>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

static constexpr int kFirstThrottleTimeout = 10;
static constexpr int kDefaultThrottleTimeout = 200;

// DevToolsFileWatcher::SharedFileWatcher --------------------------------------

class DevToolsFileWatcher::SharedFileWatcher
    : public base::RefCounted<SharedFileWatcher>,
      public base::trace_event::MemoryDumpProvider {
 public:
  SharedFileWatcher();

  void AddListener(DevToolsFileWatcher* watcher);
  void RemoveListener(DevToolsFileWatcher* watcher);
  void AddWatch(const base::FilePath& path);
  void RemoveWatch(const base::FilePath& path);

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(
      const base::trace_event::MemoryDumpArgs& args,
      base::trace_event::ProcessMemoryDump* process_memory_dump) override;

 private:
  friend class base::RefCounted<SharedFileWatcher>;
  ~SharedFileWatcher() override;

  using FilePathTimesMap = std::unordered_map<base::FilePath, base::Time>;
  FilePathTimesMap GetModificationTimes(const base::FilePath& path);
  void DirectoryChanged(const base::FilePath& path, bool error);
  void DispatchNotifications();

  std::vector<raw_ptr<DevToolsFileWatcher, VectorExperimental>> listeners_;
  std::map<base::FilePath, std::unique_ptr<base::FilePathWatcher>> watchers_;
  std::map<base::FilePath, FilePathTimesMap> file_path_times_;
  std::set<base::FilePath> pending_paths_;
  base::Time last_event_time_;
  base::TimeDelta last_dispatch_cost_;
  SEQUENCE_CHECKER(sequence_checker_);
};

DevToolsFileWatcher::SharedFileWatcher::SharedFileWatcher()
    : last_dispatch_cost_(base::Milliseconds(kDefaultThrottleTimeout)) {
  DevToolsFileWatcher::s_shared_watcher_ = this;
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "DevTools", base::SequencedTaskRunner::GetCurrentDefault(),
          base::trace_event::MemoryDumpProvider::Options());
}

DevToolsFileWatcher::SharedFileWatcher::~SharedFileWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  DevToolsFileWatcher::s_shared_watcher_ = nullptr;
}

bool DevToolsFileWatcher::SharedFileWatcher::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int index = 0;
  for (auto& file_path : file_path_times_) {
    size_t file_paths_size = 0;
    for (auto& path_and_time : file_path.second) {
      file_paths_size += path_and_time.first.value().length() *
                         sizeof(base::FilePath::StringType::value_type);
    }
    auto* dump = process_memory_dump->CreateAllocatorDump(
        base::StringPrintf("devtools/file_watcher_0x%x", index++));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    file_path.second.size());
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    file_paths_size);
  }
  return true;
}

void DevToolsFileWatcher::SharedFileWatcher::AddListener(
    DevToolsFileWatcher* watcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  listeners_.push_back(watcher);
}

void DevToolsFileWatcher::SharedFileWatcher::RemoveListener(
    DevToolsFileWatcher* watcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = base::ranges::find(listeners_, watcher);
  listeners_.erase(it);
  if (listeners_.empty()) {
    file_path_times_.clear();
    pending_paths_.clear();
  }
}

void DevToolsFileWatcher::SharedFileWatcher::AddWatch(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (watchers_.find(path) != watchers_.end())
    return;
  if (!base::FilePathWatcher::RecursiveWatchAvailable())
    return;
  watchers_[path] = std::make_unique<base::FilePathWatcher>();
  bool success = watchers_[path]->Watch(
      path, base::FilePathWatcher::Type::kRecursive,
      base::BindRepeating(&SharedFileWatcher::DirectoryChanged,
                          base::Unretained(this)));
  if (!success)
    return;

  file_path_times_[path] = GetModificationTimes(path);
}

DevToolsFileWatcher::SharedFileWatcher::FilePathTimesMap
DevToolsFileWatcher::SharedFileWatcher::GetModificationTimes(
    const base::FilePath& path) {
  FilePathTimesMap times_map;
  base::FileEnumerator enumerator(path, true, base::FileEnumerator::FILES);
  base::FilePath file_path = enumerator.Next();
  while (!file_path.empty()) {
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    times_map[std::move(file_path)] = file_info.GetLastModifiedTime();
    file_path = enumerator.Next();
  }
  return times_map;
}

void DevToolsFileWatcher::SharedFileWatcher::RemoveWatch(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watchers_.erase(path);
  file_path_times_.erase(path);
}

void DevToolsFileWatcher::SharedFileWatcher::DirectoryChanged(
    const base::FilePath& path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pending_paths_.insert(path);
  if (pending_paths_.size() > 1)
    return;  // PostDelayedTask is already pending.

  base::Time now = base::Time::Now();
  // Quickly dispatch first chunk.
  base::TimeDelta shedule_for = now - last_event_time_ > last_dispatch_cost_
                                    ? base::Milliseconds(kFirstThrottleTimeout)
                                    : last_dispatch_cost_ * 2;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DevToolsFileWatcher::SharedFileWatcher::DispatchNotifications, this),
      shedule_for);
  last_event_time_ = now;
}

void DevToolsFileWatcher::SharedFileWatcher::DispatchNotifications() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_paths_.empty())
    return;
  base::Time start = base::Time::Now();
  std::vector<std::string> added_paths;
  std::vector<std::string> removed_paths;
  std::vector<std::string> changed_paths;

  for (const auto& pending_path : pending_paths_) {
    FilePathTimesMap& old_times = file_path_times_[pending_path];
    FilePathTimesMap current_times = GetModificationTimes(pending_path);
    for (const auto& path_time : current_times) {
      const base::FilePath& path = path_time.first;
      auto old_timestamp = old_times.find(path);
      if (old_timestamp == old_times.end())
        added_paths.push_back(path.AsUTF8Unsafe());
      else if (old_timestamp->second != path_time.second)
        changed_paths.push_back(path.AsUTF8Unsafe());
    }
    for (const auto& path_time : old_times) {
      const base::FilePath& path = path_time.first;
      if (current_times.find(path) == current_times.end())
        removed_paths.push_back(path.AsUTF8Unsafe());
    }
    old_times.swap(current_times);
  }
  pending_paths_.clear();

  for (DevToolsFileWatcher* watcher : listeners_) {
    watcher->client_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(watcher->callback_, changed_paths,
                                  added_paths, removed_paths));
  }
  last_dispatch_cost_ = base::Time::Now() - start;
}

// DevToolsFileWatcher ---------------------------------------------------------

namespace {
base::SequencedTaskRunner* impl_task_runner() {
  constexpr base::TaskTraits kImplTaskTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT};
  static base::LazyThreadPoolSequencedTaskRunner s_file_task_runner =
      LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(kImplTaskTraits);
  return s_file_task_runner.Get().get();
}
}  // namespace

// static
DevToolsFileWatcher::SharedFileWatcher* DevToolsFileWatcher::s_shared_watcher_;

// static
void DevToolsFileWatcher::Deleter::operator()(const DevToolsFileWatcher* ptr) {
  impl_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DevToolsFileWatcher::Destroy, base::Unretained(ptr)));
}

DevToolsFileWatcher::DevToolsFileWatcher(
    WatchCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
    : callback_(std::move(callback)),
      client_task_runner_(std::move(callback_task_runner)) {
  impl_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsFileWatcher::InitSharedWatcher,
                                base::Unretained(this)));
}

DevToolsFileWatcher::~DevToolsFileWatcher() {
  DCHECK(impl_task_runner()->RunsTasksInCurrentSequence());
  shared_watcher_->RemoveListener(this);
}

void DevToolsFileWatcher::InitSharedWatcher() {
  if (!DevToolsFileWatcher::s_shared_watcher_)
    new SharedFileWatcher();
  shared_watcher_ = DevToolsFileWatcher::s_shared_watcher_;
  shared_watcher_->AddListener(this);
}

void DevToolsFileWatcher::AddWatch(base::FilePath path) {
  impl_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsFileWatcher::AddWatchOnImpl,
                                base::Unretained(this), std::move(path)));
}

void DevToolsFileWatcher::RemoveWatch(base::FilePath path) {
  impl_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DevToolsFileWatcher::RemoveWatchOnImpl,
                                base::Unretained(this), std::move(path)));
}

void DevToolsFileWatcher::AddWatchOnImpl(base::FilePath path) {
  shared_watcher_->AddWatch(std::move(path));
}

void DevToolsFileWatcher::RemoveWatchOnImpl(base::FilePath path) {
  shared_watcher_->RemoveWatch(std::move(path));
}
