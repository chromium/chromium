// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/exo_parts.h"

#include "base/memory/ptr_util.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_switches.h"
#include "components/exo/file_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/drop_data.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace {

constexpr char kMimeTypeArcUriList[] = "application/x-arc-uri-list";

storage::FileSystemContext* GetFileSystemContext() {
  // Obtains the primary profile.
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  Profile* primary_profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  if (!primary_profile)
    return nullptr;

  return file_manager::util::GetFileSystemContextForExtensionId(
      primary_profile, file_manager::kFileManagerAppId);
}

void GetFileSystemUrlsFromPickle(
    const base::Pickle& pickle,
    std::vector<storage::FileSystemURL>* file_system_urls) {
  storage::FileSystemContext* file_system_context = GetFileSystemContext();
  if (!file_system_context)
    return;

  std::vector<content::DropData::FileSystemFileInfo> file_system_files;
  if (!content::DropData::FileSystemFileInfo::ReadFileSystemFilesFromPickle(
          pickle, &file_system_files))
    return;

  for (const auto& file_system_file : file_system_files) {
    const storage::FileSystemURL file_system_url =
        file_system_context->CrackURL(file_system_file.url);
    if (file_system_url.is_valid())
      file_system_urls->push_back(file_system_url);
  }
}

class ChromeFileHelper : public exo::FileHelper {
 public:
  ChromeFileHelper() = default;
  ~ChromeFileHelper() override = default;

  // exo::FileHelper:
  std::string GetMimeTypeForUriList() const override {
    return kMimeTypeArcUriList;
  }

  bool GetUrlFromPath(const std::string& app_id,
                      const base::FilePath& path,
                      GURL* out) override {
    return file_manager::util::ConvertPathToArcUrl(path, out);
  }

  bool HasUrlsInPickle(const base::Pickle& pickle) override {
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
    return !file_system_urls.empty();
  }

  void GetUrlsFromPickle(const std::string& app_id,
                         const base::Pickle& pickle,
                         UrlsFromPickleCallback callback) override {
    std::vector<storage::FileSystemURL> file_system_urls;
    GetFileSystemUrlsFromPickle(pickle, &file_system_urls);
    if (file_system_urls.empty()) {
      std::move(callback).Run(std::vector<GURL>());
      return;
    }
    file_manager::util::ConvertToContentUrls(file_system_urls,
                                             std::move(callback));
  }
};

}  // namespace

// static
std::unique_ptr<ExoParts> ExoParts::CreateIfNecessary() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshEnableWaylandServer)) {
    return nullptr;
  }

  return base::WrapUnique(new ExoParts());
}

ExoParts::~ExoParts() {
  ash::Shell::Get()->DestroyWaylandServer();
}

ExoParts::ExoParts() {
  std::unique_ptr<ChromeFileHelper> file_helper =
      std::make_unique<ChromeFileHelper>();
  ash::Shell::Get()->InitWaylandServer(std::move(file_helper));
}
