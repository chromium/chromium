// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"

#include <stddef.h>

#include <vector>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {
namespace file_system_provider {
namespace util {

namespace {

// Root mount path for all of the provided file systems.
const base::FilePath::CharType kProvidedMountPointRoot[] =
    FILE_PATH_LITERAL("/provided");

}  // namespace

// Escapes the file system id so it can be used as a file/directory name.
// This is based on net/base/escape.cc: net::(anonymous namespace)::Escape
std::string EscapeFileSystemId(const std::string& file_system_id) {
  std::string escaped;
  for (size_t i = 0; i < file_system_id.size(); ++i) {
    const char c = file_system_id[i];
    if (c == '%' || c == '.' || c == '/') {
      base::StringAppendF(&escaped, "%%%02X", c);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

base::FilePath GetMountPath(Profile* profile,
                            const ProviderId& provider_id,
                            const std::string& file_system_id) {
  const user_manager::User* const user =
      user_manager::UserManager::IsInitialized()
          ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : NULL;
  const std::string safe_file_system_id = EscapeFileSystemId(file_system_id);
  const std::string username_suffix = user ? user->username_hash() : "";
  return base::FilePath(kProvidedMountPointRoot)
      .AppendASCII(provider_id.ToString() + ":" + safe_file_system_id + ":" +
                   username_suffix);
}

bool IsFileSystemProviderLocalPath(const base::FilePath& local_path) {
  std::vector<base::FilePath::StringType> components;
  local_path.GetComponents(&components);

  if (components.size() < 3)
    return false;

  if (components[0] != FILE_PATH_LITERAL("/"))
    return false;

  if (components[1] != kProvidedMountPointRoot + 1 /* no leading slash */)
    return false;

  return true;
}

FileSystemURLParser::FileSystemURLParser(const storage::FileSystemURL& url)
    : url_(url), file_system_(NULL) {
}

FileSystemURLParser::~FileSystemURLParser() {
}

bool FileSystemURLParser::Parse() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (url_.type() != storage::kFileSystemTypeProvided)
    return false;

  // First, find the service handling the mount point of the URL.
  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();

  for (size_t i = 0; i < profiles.size(); ++i) {
    Profile* original_profile = profiles[i]->GetOriginalProfile();

    if (original_profile != profiles[i] ||
        !chromeos::ProfileHelper::IsRegularProfile(original_profile)) {
      continue;
    }

    Service* const service = Service::Get(original_profile);
    if (!service)
      continue;

    ProvidedFileSystemInterface* const file_system =
        service->GetProvidedFileSystem(url_.filesystem_id());
    if (!file_system)
      continue;

    // Strip the mount path name from the local path, to extract the file path
    // within the provided file system.
    file_system_ = file_system;
    std::vector<base::FilePath::StringType> components;
    url_.path().GetComponents(&components);
    if (components.size() < 3)
      return false;

    file_path_ = base::FilePath(FILE_PATH_LITERAL("/"));
    for (size_t i = 3; i < components.size(); ++i) {
      // TODO(mtomasz): This could be optimized, to avoid unnecessary copies.
      file_path_ = file_path_.Append(components[i]);
    }

    return true;
  }

  // Nothing has been found.
  return false;
}

LocalPathParser::LocalPathParser(Profile* profile,
                                 const base::FilePath& local_path)
    : profile_(profile), local_path_(local_path), file_system_(NULL) {
}

LocalPathParser::~LocalPathParser() {
}

bool LocalPathParser::Parse() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!IsFileSystemProviderLocalPath(local_path_))
    return false;

  std::vector<base::FilePath::StringType> components;
  local_path_.GetComponents(&components);

  if (components.size() < 3)
    return false;

  const std::string mount_point_name = components[2];

  Service* const service = Service::Get(profile_);
  if (!service)
    return false;

  ProvidedFileSystemInterface* const file_system =
      service->GetProvidedFileSystem(mount_point_name);
  if (!file_system)
    return false;

  // Strip the mount point path from the virtual path, to extract the file
  // path within the provided file system.
  file_system_ = file_system;
  file_path_ = base::FilePath(FILE_PATH_LITERAL("/"));
  for (size_t i = 3; i < components.size(); ++i) {
    file_path_ = file_path_.Append(components[i]);
  }

  return true;
}

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos
