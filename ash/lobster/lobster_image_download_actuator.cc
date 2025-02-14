// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_download_actuator.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr int kQueryCharLimit = 230;

}  // namespace

LobsterImageDownloadActuator::LobsterImageDownloadActuator()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

LobsterImageDownloadActuator::~LobsterImageDownloadActuator() = default;

void LobsterImageDownloadActuator::WriteImageToPath(
    const base::FilePath& save_dir,
    const std::string& query,
    uint32_t id,
    const std::string& image_bytes,
    LobsterImageDownloadResponseCallback callback) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const base::FilePath& save_dir, const std::string& query,
             uint32_t id,
             std::string_view image_bytes) -> LobsterImageDownloadResponse {
            std::string sanitized_file_name = query;

            // Makes the file name valid.
            base::i18n::ReplaceIllegalCharactersInPath(&sanitized_file_name,
                                                       '-');
            std::string trimmed_file_name =
                sanitized_file_name.substr(0, kQueryCharLimit);

            base::FilePath saved_file_path =
                base::FeatureList::IsEnabled(
                    features::kLobsterFileNamingImprovement)
                    ? base::GetUniquePathWithSuffixFormat(
                          save_dir.Append(
                              base::StringPrintf("%s.jpeg", trimmed_file_name)),
                          "-%d")
                    : save_dir.Append(base::StringPrintf(
                          "%s-%d.jpeg", trimmed_file_name, id));

            if (saved_file_path == base::FilePath() ||
                base::PathExists(saved_file_path)) {
              LOG(ERROR) << "File name already exists: " << saved_file_path;
              return {.download_path = saved_file_path, .success = false};
            }

            if (base::WriteFile(saved_file_path, image_bytes)) {
              return {.download_path = saved_file_path, .success = true};
            }

            LOG(ERROR) << "Unable to write file name " << saved_file_path;
            return {.download_path = saved_file_path, .success = false};
          },
          save_dir, query, id, image_bytes),
      std::move(callback));
}

}  // namespace ash
