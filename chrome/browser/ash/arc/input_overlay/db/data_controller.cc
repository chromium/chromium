// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/db/data_controller.h"

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"

namespace arc {
namespace input_overlay {
namespace {

// Base directory for saving customized data in the user profile.
constexpr char kPath[] = "google_gio";

}  // namespace

DataController::DataController(
    content::BrowserContext& browser_context,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = task_runner;
  Profile* profile = Profile::FromBrowserContext(&browser_context);
  ProfileKey* profile_key = profile->GetProfileKey();
  storage_dir_ = profile_key->GetPath().Append(kPath);
}

DataController::~DataController() = default;

base::FilePath DataController::GetFilePathFromPackageName(
    const std::string& package_name) {
  base::FilePath file_path(storage_dir_);
  return file_path.Append(package_name);
}

std::unique_ptr<AppDataProto> DataController::ReadProtoFromFile(
    const std::string& package_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto base_path = CreateOrGetDirectory();
  if (!base_path)
    return nullptr;
  auto file_path = GetFilePathFromPackageName(package_name);

  if (!ProtoFileExits(file_path)) {
    CreateEmptyFile(file_path);
    return nullptr;
  }

  std::string out;
  std::unique_ptr<AppDataProto> proto = std::make_unique<AppDataProto>();
  if (!base::ReadFileToString(file_path, &out) ||
      !(proto->ParseFromString(out))) {
    return nullptr;
  }
  return proto;
}

bool DataController::WriteProtoToFile(std::unique_ptr<AppDataProto> proto,
                                      const std::string& package_name) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  std::string proto_str;
  if (!proto->SerializeToString(&proto_str))
    return false;
  return base::WriteFile(GetFilePathFromPackageName(package_name),
                         proto_str.data(), proto_str.size()) > 0;
}

absl::optional<base::FilePath> DataController::CreateOrGetDirectory() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (base::PathExists(storage_dir_))
    return storage_dir_;
  if (base::CreateDirectory(storage_dir_))
    return storage_dir_;

  LOG(ERROR) << "Failed to create the base storage directory: "
             << storage_dir_.value();
  return absl::nullopt;
}

bool DataController::ProtoFileExits(base::FilePath file_path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return base::PathExists(file_path);
}

void DataController::CreateEmptyFile(base::FilePath file_path) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  FILE* file = base::OpenFile(file_path, "wb+");
  if (file == nullptr) {
    LOG(ERROR) << "Failed to create file: " << file_path.value();
    return;
  }
  base::CloseFile(file);
}

}  // namespace input_overlay
}  // namespace arc
