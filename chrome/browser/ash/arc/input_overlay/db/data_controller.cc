// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/db/data_controller.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"

namespace arc::input_overlay {
namespace {

// Base directory for saving customized data in the user profile.
constexpr char kPath[] = "google_gio";

std::optional<base::FilePath> CreateOrGetDirectory(
    const base::FilePath& storage_dir) {
  if (base::PathExists(storage_dir)) {
    return storage_dir;
  }
  if (base::CreateDirectory(storage_dir)) {
    return storage_dir;
  }

  LOG(ERROR) << "Failed to create the base storage directory: "
             << storage_dir.value();
  return std::nullopt;
}

bool ProtoFileExists(const base::FilePath& file_path) {
  return base::PathExists(file_path);
}

void CreateEmptyFile(const base::FilePath& file_path) {
  if (FILE* file = base::OpenFile(file_path, "wb+")) {
    base::CloseFile(file);
    return;
  }

  LOG(ERROR) << "Failed to create file: " << file_path.value();
}

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
  // `package_name` is provided by the ARC side and must stay a single path
  // component so that the data file is located directly inside of
  // `storage_dir_`.
  const base::FilePath package_path(package_name);
  if (package_path.empty() || package_path.IsAbsolute() ||
      package_path.ReferencesParent() ||
      package_path != package_path.BaseName() ||
      package_path.value() == base::FilePath::kCurrentDirectory) {
    LOG(ERROR) << "Invalid package name {" << package_name
               << "} for the data file path.";
    return base::FilePath();
  }
  return storage_dir_.Append(package_path);
}

std::unique_ptr<AppDataProto> DataController::ReadProtoFromFile(
    base::FilePath file_path) {
  if (file_path.empty()) {
    return nullptr;
  }

  if (auto base_path = CreateOrGetDirectory(file_path.DirName()); !base_path) {
    return nullptr;
  }

  if (!ProtoFileExists(file_path)) {
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
                                      base::FilePath file_path) {
  if (file_path.empty()) {
    return false;
  }

  std::string proto_str;
  if (!proto->SerializeToString(&proto_str)) {
    return false;
  }
  return base::WriteFile(file_path, proto_str);
}

}  // namespace arc::input_overlay
