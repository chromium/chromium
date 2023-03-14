// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list/crash_upload_list_chromeos.h"

#include <string>
#include <utility>

namespace {
constexpr char kJsonLogKeyFatalCrashType[] = "fatal_crash_type";
}  // namespace

CrashUploadListChromeOS::CrashUploadListChromeOS(
    const base::FilePath& upload_log_path)
    : TextLogUploadList(upload_log_path) {}

CrashUploadListChromeOS::~CrashUploadListChromeOS() = default;

std::unique_ptr<UploadList::UploadInfo>
CrashUploadListChromeOS::TryParseJsonLogEntry(const base::Value::Dict& dict) {
  auto upload_info = std::make_unique<CrashUploadInfo>(
      std::move(*TextLogUploadList::TryParseJsonLogEntry(dict)));

  if (const std::string* fatal_crash_type =
          dict.FindString(kJsonLogKeyFatalCrashType)) {
    if (*fatal_crash_type == "kernel") {
      upload_info->fatal_crash_type = CrashUploadInfo::FatalCrashType::Kernel;
    } else if (*fatal_crash_type == "ec") {
      upload_info->fatal_crash_type =
          CrashUploadInfo::FatalCrashType::EmbeddedController;
    }  // Otherwise upload_info->fatal_crash_type remains as Unknown.
  }

  return upload_info;
}

CrashUploadListChromeOS::CrashUploadInfo::CrashUploadInfo(
    TextLogUploadList::UploadInfo&& upload_info)
    : TextLogUploadList::UploadInfo(std::move(upload_info)) {}
