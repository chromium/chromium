// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/freezer_cgroup_process_manager.h"

#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {
const char kFreezerPath[] = "/sys/fs/cgroup/freezer/ui/chrome_renderers";
const char kToBeFrozen[] = "to_be_frozen";
const char kFreezerState[] = "freezer.state";
const char kCgroupProcs[] = "cgroup.procs";

const char kFreezeCommand[] = "FROZEN";
const char kThawCommand[] = "THAWED";
}  // namespace

class FreezerCgroupProcessManager::FileWorker {
 public:
  // Called on UI thread.
  explicit FileWorker(scoped_refptr<base::SequencedTaskRunner> file_thread)
      : ui_thread_(content::GetUIThreadTaskRunner({})),
        file_thread_(file_thread),
        enabled_(false),
        froze_successfully_(false) {
    DCHECK(ui_thread_->RunsTasksInCurrentSequence());
  }

  FileWorker(const FileWorker&) = delete;
  FileWorker& operator=(const FileWorker&) = delete;

  // Called on FILE thread.
  virtual ~FileWorker() { DCHECK(file_thread_->RunsTasksInCurrentSequence()); }

  void Start() {
    DCHECK(file_thread_->RunsTasksInCurrentSequence());

    default_control_path_ = base::FilePath(kFreezerPath).Append(kCgroupProcs);
    to_be_frozen_control_path_ = base::FilePath(kFreezerPath)
                                     .AppendASCII(kToBeFrozen)
                                     .AppendASCII(kCgroupProcs);
    to_be_frozen_state_path_ = base::FilePath(kFreezerPath)
                                   .AppendASCII(kToBeFrozen)
                                   .AppendASCII(kFreezerState);
    enabled_ = base::PathIsWritable(default_control_path_) &&
               base::PathIsWritable(to_be_frozen_control_path_) &&
               base::PathIsWritable(to_be_frozen_state_path_);

    if (!enabled_) {
      LOG_IF(WARNING, base::SysInfo::IsRunningOnChromeOS())
          << "Cgroup freezer does not exist or is not writable. "
          << "Unable to freeze renderer processes.";
      return;
    }

    // Thaw renderers on startup. This helps robustness for the case where we
    // start up with renderers in frozen state, for example after the previous
    // Chrome process crashed at a point in time after suspend where it still
    // hadn't thawed renderers yet.
    ThawRenderers(base::DoNothing());
  }

  void SetShouldFreezeRenderer(base::ProcessHandle handle, bool frozen) {
    DCHECK(file_thread_->RunsTasksInCurrentSequence());

    WriteCommandToFile(
        base::NumberToString(handle),
        frozen ? to_be_frozen_control_path_ : default_control_path_);
  }

  void FreezeRenderers() {
    DCHECK(file_thread_->RunsTasksInCurrentSequence());

    if (!enabled_) {
      LOG(ERROR) << "Attempting to freeze renderers when the freezer cgroup is "
                 << "not available.";
      return;
    }

    froze_successfully_ =
        WriteCommandToFile(kFreezeCommand, to_be_frozen_state_path_);
  }

  void ThawRenderers(ResultCallback callback) {
    DCHECK(file_thread_->RunsTasksInCurrentSequence());

    if (!enabled_) {
      LOG(ERROR) << "Attempting to thaw renderers when the freezer cgroup is "
                 << "not available.";
      return;
    }

    bool result = WriteCommandToFile(kThawCommand, to_be_frozen_state_path_);

    // TODO(derat): For now, lie and report success if thawing failed but
    // freezing also failed previously. Remove after weird EBADF and ENOENT
    // problems tracked at http://crbug.com/661310 are fixed.
    if (!result && !froze_successfully_)
      result = true;

    ui_thread_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), result));
  }

  void CheckCanFreezeRenderers(ResultCallback callback) {
    DCHECK(file_thread_->RunsTasksInCurrentSequence());

    ui_thread_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), enabled_));
  }

 private:
  bool WriteCommandToFile(const std::string& command,
                          const base::FilePath& file) {
    if (!base::WriteFile(file, command)) {
      PLOG(ERROR) << "Writing " << command << " to " << file.value()
                  << " failed";
      return false;
    }
    return true;
  }

  scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  scoped_refptr<base::SequencedTaskRunner> file_thread_;

  // Control path for the cgroup that is not frozen.
  base::FilePath default_control_path_;

  // Control and state paths for the cgroup whose processes will be frozen.
  base::FilePath to_be_frozen_control_path_;
  base::FilePath to_be_frozen_state_path_;

  bool enabled_;

  // True iff FreezeRenderers() wrote its command successfully the last time it
  // was called.
  bool froze_successfully_;
};

FreezerCgroupProcessManager::FreezerCgroupProcessManager()
    : file_thread_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock()})),
      file_worker_(new FileWorker(file_thread_)) {
  file_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&FileWorker::Start, base::Unretained(file_worker_.get())));
}

FreezerCgroupProcessManager::~FreezerCgroupProcessManager() {
  file_thread_->DeleteSoon(FROM_HERE, file_worker_.release());
}

void FreezerCgroupProcessManager::SetShouldFreezeRenderer(
    base::ProcessHandle handle,
    bool frozen) {
  file_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&FileWorker::SetShouldFreezeRenderer,
                     base::Unretained(file_worker_.get()), handle, frozen));
}

void FreezerCgroupProcessManager::FreezeRenderers() {
  file_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&FileWorker::FreezeRenderers,
                                        base::Unretained(file_worker_.get())));
}

void FreezerCgroupProcessManager::ThawRenderers(ResultCallback callback) {
  file_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&FileWorker::ThawRenderers,
                                        base::Unretained(file_worker_.get()),
                                        std::move(callback)));
}

void FreezerCgroupProcessManager::CheckCanFreezeRenderers(
    ResultCallback callback) {
  file_thread_->PostTask(FROM_HERE,
                         base::BindOnce(&FileWorker::CheckCanFreezeRenderers,
                                        base::Unretained(file_worker_.get()),
                                        std::move(callback)));
}

}  // namespace ash
