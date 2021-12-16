// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_party_sets/first_party_sets_util.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"

namespace {

constexpr base::FilePath::CharType kPersistedFirstPartySetsFileName[] =
    FILE_PATH_LITERAL("persisted_first_party_sets.json");

// Reads the sets as raw JSON from their storage file, returning the raw sets on
// success and empty string on failure.
std::string LoadSetsFromDisk(const base::FilePath& path) {
  DCHECK(!path.empty());

  std::string result;
  if (!base::ReadFileToString(path, &result)) {
    VLOG(1) << "Failed loading serialized First-Party Sets file from "
            << path.MaybeAsASCII();
    return "";
  }
  return result;
}

// Writes the sets as raw JSON to the storage file.
//
// TODO(crbug.com/1219656): To handle the cases of file corrupting due to
// incomplete writes, write to a temp file then rename over the old file.
void MaybeWriteSetsToDisk(const base::FilePath& path, const std::string& sets) {
  DCHECK(!path.empty());

  if (!base::WriteFile(path, sets)) {
    VLOG(1) << "Failed writing serialized First-Party Sets to file "
            << path.MaybeAsASCII();
  }
}

void OnGetUpdatedSets(const base::FilePath& path, const std::string& sets) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&MaybeWriteSetsToDisk, path, sets));
}

void SendPersistedSets(
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets,
    const base::FilePath& path,
    const std::string& sets) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(send_sets).Run(base::BindOnce(&OnGetUpdatedSets, path), sets);
}

}  // namespace

// static
FirstPartySetsUtil* FirstPartySetsUtil::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static base::NoDestructor<FirstPartySetsUtil> instance;
  return instance.get();
}

void FirstPartySetsUtil::SendAndUpdatePersistedSets(
    const base::FilePath& user_data_dir,
    base::OnceCallback<void(base::OnceCallback<void(const std::string&)>,
                            const std::string&)> send_sets) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!user_data_dir.empty());
  const base::FilePath persisted_sets_path =
      user_data_dir.Append(kPersistedFirstPartySetsFileName);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadSetsFromDisk, persisted_sets_path),
      base::BindOnce(&SendPersistedSets, std::move(send_sets),
                     persisted_sets_path));
}
