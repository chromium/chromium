// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/feedback_zip_builder.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/zlib/google/zip.h"

namespace selection {

struct FeedbackZipBuilder::State {};

FeedbackZipBuilder::FeedbackZipBuilder() : state_(std::make_unique<State>()) {}
FeedbackZipBuilder::FeedbackZipBuilder(FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder& FeedbackZipBuilder::operator=(
    FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder::~FeedbackZipBuilder() = default;

void FeedbackZipBuilder::CreateZip(
    base::FilePath target_zip_path,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback) const {
  CHECK(state_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&FeedbackZipBuilder::CreateZipOnThreadPool,
                     std::move(target_zip_path), *state_),
      std::move(callback));
}

// static
std::optional<base::FilePath> FeedbackZipBuilder::CreateZipOnThreadPool(
    base::FilePath target_zip_path,
    State state) {
  if (target_zip_path.empty() ||
      !base::CreateDirectory(target_zip_path.DirName())) {
    return std::nullopt;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return std::nullopt;
  }

  base::FilePath zip_path = base::GetUniquePath(target_zip_path);
  if (zip_path.empty()) {
    return std::nullopt;
  }
  if (!zip::Zip(temp_dir.GetPath(), zip_path, /*include_hidden_files=*/false)) {
    base::DeleteFile(zip_path);
    return std::nullopt;
  }
  return zip_path;
}

}  // namespace selection
