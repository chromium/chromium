// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/devtools/devtools_file_system_indexer.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <set>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "content/public/browser/browser_thread.h"

using base::FileEnumerator;
using base::FilePath;
using base::Time;
using base::TimeTicks;
using content::BrowserThread;
using std::map;
using std::string;
using std::vector;

namespace {

using std::set;

base::SequencedTaskRunner* impl_task_runner() {
  constexpr base::TaskTraits kBlockingTraits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT};
  static base::LazyThreadPoolSequencedTaskRunner s_sequenced_task_task_runner =
      LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(kBlockingTraits);
  return s_sequenced_task_task_runner.Get().get();
}

typedef int32_t Trigram;
typedef char TrigramChar;
typedef uint16_t FileId;

const int kMinTimeoutBetweenWorkedNotification = 200;
// Trigram characters include all ASCII printable characters (32-126) except for
// the capital letters, because the index is case insensitive.
const size_t kTrigramCharacterCount = 126 - 'Z' - 1 + 'A' - ' ' + 1;
const size_t kTrigramCount =
    kTrigramCharacterCount * kTrigramCharacterCount * kTrigramCharacterCount;
const int kMaxReadLength = 10 * 1024;
const TrigramChar kUndefinedTrigramChar = -1;
const TrigramChar kBinaryTrigramChar = -2;
const Trigram kUndefinedTrigram = -1;

class Index {
 public:
  Index();

  Index(const Index&) = delete;
  Index& operator=(const Index&) = delete;

  // Index is only instantiated as a leak LazyInstance, so the destructor is
  // never called.
  ~Index() = delete;

  Time LastModifiedTimeForFile(const FilePath& file_path);
  void SetTrigramsForFile(const FilePath& file_path,
                          const vector<Trigram>& index,
                          const Time& time);
  vector<FilePath> Search(const string& query);
  void NormalizeVectors();
  void Reset();
  void EnsureInitialized();

 private:
  FileId GetFileId(const FilePath& file_path);

  typedef map<FilePath, FileId> FileIdsMap;
  FileIdsMap file_ids_;
  FileId last_file_id_;
  // The index in this vector is the trigram id.
  vector<vector<FileId> > index_;
  typedef map<FilePath, Time> IndexedFilesMap;
  IndexedFilesMap index_times_;
  vector<bool> is_normalized_;
  SEQUENCE_CHECKER(sequence_checker_);
};

base::LazyInstance<Index>::Leaky g_trigram_index = LAZY_INSTANCE_INITIALIZER;

TrigramChar TrigramCharForChar(char c) {
  static TrigramChar* trigram_chars = nullptr;
  if (!trigram_chars) {
    trigram_chars = new TrigramChar[256];
    for (size_t i = 0; i < 256; ++i) {
      if (i > 127) {
        trigram_chars[i] = kUndefinedTrigramChar;
        continue;
      }
      char ch = static_cast<char>(i);
      if (ch == '\t')
        ch = ' ';
      if (base::IsAsciiUpper(ch))
        ch = ch - 'A' + 'a';

      bool is_binary_char = ch < 9 || (ch >= 14 && ch < 32) || ch == 127;
      if (is_binary_char) {
        trigram_chars[i] = kBinaryTrigramChar;
        continue;
      }

      if (ch < ' ') {
        trigram_chars[i] = kUndefinedTrigramChar;
        continue;
      }

      if (ch >= 'Z')
        ch = ch - 'Z' - 1 + 'A';
      ch -= ' ';
      char signed_trigram_count = static_cast<char>(kTrigramCharacterCount);
      CHECK(ch >= 0 && ch < signed_trigram_count);
      trigram_chars[i] = ch;
    }
  }
  unsigned char uc = static_cast<unsigned char>(c);
  return trigram_chars[uc];
}

Trigram TrigramAtIndex(const vector<TrigramChar>& trigram_chars, size_t index) {
  static int kTrigramCharacterCountSquared =
      kTrigramCharacterCount * kTrigramCharacterCount;
  if (trigram_chars[index] == kUndefinedTrigramChar ||
      trigram_chars[index + 1] == kUndefinedTrigramChar ||
      trigram_chars[index + 2] == kUndefinedTrigramChar)
    return kUndefinedTrigram;
  Trigram trigram = kTrigramCharacterCountSquared * trigram_chars[index] +
                    kTrigramCharacterCount * trigram_chars[index + 1] +
                    trigram_chars[index + 2];
  return trigram;
}

Index::Index() : last_file_id_(0) {
  Reset();
}

void Index::Reset() {
  file_ids_.clear();
  index_.clear();
  index_times_.clear();
  is_normalized_.clear();
  last_file_id_ = 0;
}

void Index::EnsureInitialized() {
  if (!index_.empty())
    return;
  index_.resize(kTrigramCount);
  is_normalized_.resize(kTrigramCount);
  std::fill(is_normalized_.begin(), is_normalized_.end(), true);
}

Time Index::LastModifiedTimeForFile(const FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureInitialized();
  Time last_modified_time;
  if (index_times_.find(file_path) != index_times_.end())
    last_modified_time = index_times_[file_path];
  return last_modified_time;
}

void Index::SetTrigramsForFile(const FilePath& file_path,
                               const vector<Trigram>& index,
                               const Time& time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureInitialized();
  FileId file_id = GetFileId(file_path);
  auto it = index.begin();
  for (; it != index.end(); ++it) {
    Trigram trigram = *it;
    index_[trigram].push_back(file_id);
    is_normalized_[trigram] = false;
  }
  index_times_[file_path] = time;
}

vector<FilePath> Index::Search(const string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureInitialized();
  const char* data = query.c_str();
  vector<TrigramChar> trigram_chars;
  trigram_chars.reserve(query.size());
  for (size_t i = 0; i < query.size(); ++i) {
      TrigramChar trigram_char = TrigramCharForChar(data[i]);
      if (trigram_char == kBinaryTrigramChar)
        trigram_char = kUndefinedTrigramChar;
      trigram_chars.push_back(trigram_char);
  }
  vector<Trigram> trigrams;
  for (size_t i = 0; i + 2 < query.size(); ++i) {
    Trigram trigram = TrigramAtIndex(trigram_chars, i);
    if (trigram != kUndefinedTrigram)
      trigrams.push_back(trigram);
  }
  set<FileId> file_ids;
  bool first = true;
  vector<Trigram>::const_iterator it = trigrams.begin();
  for (; it != trigrams.end(); ++it) {
    Trigram trigram = *it;
    if (first) {
      base::ranges::copy(index_[trigram],
                         std::inserter(file_ids, file_ids.begin()));
      first = false;
      continue;
    }
    set<FileId> intersection = base::STLSetIntersection<set<FileId> >(
        file_ids, index_[trigram]);
    file_ids.swap(intersection);
  }
  vector<FilePath> result;
  FileIdsMap::const_iterator ids_it = file_ids_.begin();
  for (; ids_it != file_ids_.end(); ++ids_it) {
    if (trigrams.empty() || file_ids.find(ids_it->second) != file_ids.end()) {
      result.push_back(ids_it->first);
    }
  }
  return result;
}

FileId Index::GetFileId(const FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureInitialized();
  string file_path_str = file_path.AsUTF8Unsafe();
  if (file_ids_.find(file_path) != file_ids_.end())
    return file_ids_[file_path];
  file_ids_[file_path] = ++last_file_id_;
  return last_file_id_;
}

void Index::NormalizeVectors() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureInitialized();
  for (size_t i = 0; i < kTrigramCount; ++i) {
    if (!is_normalized_[i]) {
      std::sort(index_[i].begin(), index_[i].end());
      if (index_[i].capacity() > index_[i].size())
        vector<FileId>(index_[i]).swap(index_[i]);
      is_normalized_[i] = true;
    }
  }
}

}  // namespace

DevToolsFileSystemIndexer::FileSystemIndexingJob::FileSystemIndexingJob(
    const FilePath& file_system_path,
    const std::vector<base::FilePath>& excluded_folders,
    TotalWorkCallback total_work_callback,
    const WorkedCallback& worked_callback,
    DoneCallback done_callback)
    : file_system_path_(file_system_path),
      excluded_folders_(excluded_folders),
      total_work_callback_(std::move(total_work_callback)),
      worked_callback_(worked_callback),
      done_callback_(std::move(done_callback)),
      files_indexed_(0),
      stopped_(false) {
  current_trigrams_set_.resize(kTrigramCount);
  current_trigrams_.reserve(kTrigramCount);
  pending_folders_.push_back(file_system_path);
}

DevToolsFileSystemIndexer::FileSystemIndexingJob::~FileSystemIndexingJob() {}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  impl_task_runner()->PostTask(
      FROM_HERE, BindOnce(&FileSystemIndexingJob::CollectFilesToIndex, this));
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::Stop() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  impl_task_runner()->PostTask(
      FROM_HERE, BindOnce(&FileSystemIndexingJob::StopOnImplSequence, this));
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::StopOnImplSequence() {
  stopped_ = true;
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::CollectFilesToIndex() {
  DCHECK(impl_task_runner()->RunsTasksInCurrentSequence());
  if (stopped_)
    return;
  if (!file_enumerator_) {
    file_enumerator_ = std::make_unique<FileEnumerator>(
        pending_folders_.back(), false,
        FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
    pending_folders_.pop_back();
  }
  FilePath file_path = file_enumerator_->Next();
  if (file_path.empty() && !pending_folders_.empty()) {
    file_enumerator_ = std::make_unique<FileEnumerator>(
        pending_folders_.back(), false,
        FileEnumerator::FILES | FileEnumerator::DIRECTORIES);
    pending_folders_.pop_back();
    impl_task_runner()->PostTask(
        FROM_HERE, BindOnce(&FileSystemIndexingJob::CollectFilesToIndex, this));
    return;
  }

  if (file_path.empty()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, BindOnce(std::move(total_work_callback_), file_path_times_.size()));
    indexing_it_ = file_path_times_.begin();
    IndexFiles();
    return;
  }
  if (file_enumerator_->GetInfo().IsDirectory()) {
    bool excluded = false;
    for (const FilePath& excluded_folder : excluded_folders_) {
      excluded = excluded_folder.IsParent(file_path);
      if (excluded)
        break;
    }
    if (!excluded)
      pending_folders_.push_back(file_path);
    impl_task_runner()->PostTask(
        FROM_HERE, BindOnce(&FileSystemIndexingJob::CollectFilesToIndex, this));
    return;
  }

  Time saved_last_modified_time =
      g_trigram_index.Get().LastModifiedTimeForFile(file_path);
  FileEnumerator::FileInfo file_info = file_enumerator_->GetInfo();
  Time current_last_modified_time = file_info.GetLastModifiedTime();
  if (current_last_modified_time >= saved_last_modified_time) {
    file_path_times_[file_path] = current_last_modified_time;
  }
  impl_task_runner()->PostTask(
      FROM_HERE, BindOnce(&FileSystemIndexingJob::CollectFilesToIndex, this));
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::IndexFiles() {
  DCHECK(impl_task_runner()->RunsTasksInCurrentSequence());
  if (stopped_)
    return;
  if (indexing_it_ == file_path_times_.end()) {
    g_trigram_index.Get().NormalizeVectors();
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_callback_));
    return;
  }
  FilePath file_path = indexing_it_->first;
  current_file_.Initialize(file_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!current_file_.IsValid()) {
    FinishFileIndexing(false);
    return;
  }
  current_file_offset_ = 0;
  current_trigrams_.clear();
  std::fill(current_trigrams_set_.begin(), current_trigrams_set_.end(), false);
  ReadFromFile();
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::ReadFromFile() {
  if (stopped_) {
    CloseFile();
    return;
  }

  auto data = base::HeapArray<uint8_t>::Uninit(kMaxReadLength);
  std::optional<size_t> bytes_read =
      current_file_.Read(current_file_offset_, data);
  if (!bytes_read.has_value()) {
    FinishFileIndexing(false);
    return;
  }

  size_t size = bytes_read.value();
  if (size < 3) {
    FinishFileIndexing(true);
    return;
  }

  vector<TrigramChar> trigram_chars;
  trigram_chars.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    TrigramChar trigram_char = TrigramCharForChar(data[i]);
    if (trigram_char == kBinaryTrigramChar) {
      current_trigrams_.clear();
      FinishFileIndexing(true);
      return;
    }
    trigram_chars.push_back(trigram_char);
  }

  for (size_t i = 0; i + 2 < size; ++i) {
    Trigram trigram = TrigramAtIndex(trigram_chars, i);
    if ((trigram != kUndefinedTrigram) && !current_trigrams_set_[trigram]) {
      current_trigrams_set_[trigram] = true;
      current_trigrams_.push_back(trigram);
    }
  }
  current_file_offset_ += size - 2;
  impl_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemIndexingJob::ReadFromFile, this));
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::FinishFileIndexing(
    bool success) {
  DCHECK(impl_task_runner()->RunsTasksInCurrentSequence());
  CloseFile();
  if (success) {
    FilePath file_path = indexing_it_->first;
    g_trigram_index.Get().SetTrigramsForFile(
        file_path, current_trigrams_, file_path_times_[file_path]);
  }
  ReportWorked();
  ++indexing_it_;
  impl_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemIndexingJob::IndexFiles, this));
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::CloseFile() {
  if (current_file_.IsValid())
    current_file_.Close();
}

void DevToolsFileSystemIndexer::FileSystemIndexingJob::ReportWorked() {
  TimeTicks current_time = TimeTicks::Now();
  bool should_send_worked_notification = true;
  if (!last_worked_notification_time_.is_null()) {
    base::TimeDelta delta = current_time - last_worked_notification_time_;
    if (delta.InMilliseconds() < kMinTimeoutBetweenWorkedNotification) {
      should_send_worked_notification = false;
    }
  }
  ++files_indexed_;
  if (should_send_worked_notification) {
    last_worked_notification_time_ = current_time;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, BindOnce(worked_callback_, files_indexed_));
    files_indexed_ = 0;
  }
}

static int g_instance_count = 0;

DevToolsFileSystemIndexer::DevToolsFileSystemIndexer() {
  impl_task_runner()->PostTask(FROM_HERE,
                               base::BindOnce([]() { ++g_instance_count; }));
}

DevToolsFileSystemIndexer::~DevToolsFileSystemIndexer() {
  impl_task_runner()->PostTask(FROM_HERE, base::BindOnce([]() {
                                 --g_instance_count;
                                 if (!g_instance_count)
                                   g_trigram_index.Get().Reset();
                               }));
}

scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>
DevToolsFileSystemIndexer::IndexPath(
    const string& file_system_path,
    const vector<string>& excluded_folders,
    TotalWorkCallback total_work_callback,
    const WorkedCallback& worked_callback,
    DoneCallback done_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  vector<base::FilePath> paths;
  for (const string& path : excluded_folders) {
    paths.push_back(FilePath::FromUTF8Unsafe(path));
  }
  scoped_refptr<FileSystemIndexingJob> indexing_job =
      new FileSystemIndexingJob(FilePath::FromUTF8Unsafe(file_system_path),
                                paths, std::move(total_work_callback),
                                worked_callback, std::move(done_callback));
  indexing_job->Start();
  return indexing_job;
}

void DevToolsFileSystemIndexer::SearchInPath(
    const std::string& file_system_path,
    const std::string& query,
    SearchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  impl_task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&DevToolsFileSystemIndexer::SearchInPathOnImplSequence, this,
               file_system_path, query, std::move(callback)));
}

void DevToolsFileSystemIndexer::SearchInPathOnImplSequence(
    const std::string& file_system_path,
    const std::string& query,
    SearchCallback callback) {
  DCHECK(impl_task_runner()->RunsTasksInCurrentSequence());
  vector<FilePath> file_paths = g_trigram_index.Get().Search(query);
  vector<string> result;
  FilePath path = FilePath::FromUTF8Unsafe(file_system_path);
  vector<FilePath>::const_iterator it = file_paths.begin();
  for (; it != file_paths.end(); ++it) {
    if (path.IsParent(*it))
      result.push_back(it->AsUTF8Unsafe());
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, BindOnce(std::move(callback), std::move(result)));
}
