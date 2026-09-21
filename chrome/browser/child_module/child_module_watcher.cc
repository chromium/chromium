// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_watcher.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/common/child_module/child_module_helper.h"

namespace child_module {

namespace {

// Debounce interval to coalesce burst filesystem notifications.
constexpr base::TimeDelta kDebounceDelay = base::Milliseconds(200);

// Periodic rescan interval as a fallback for missed filesystem events.
constexpr base::TimeDelta kPeriodicRescanInterval = base::Minutes(20);

// Scans `modules_dir` for valid, ready child module versions.
VersionSet ScanChildModules(const base::FilePath& modules_dir) {
  VersionSet versions;
  base::FileEnumerator enumerator(modules_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  enumerator.ForEach([&](const base::FilePath& dir) {
    std::string dir_name = enumerator.GetInfo().GetName().MaybeAsASCII();
    base::Version candidate(dir_name);
    if (!candidate.IsValid() || candidate.GetString() != dir_name ||
        base::IsLink(dir)) {
      return;
    }
    base::FilePath manifest_path = GetManifestPath(dir);
    base::File::Info info;
    if (!base::GetFileInfo(manifest_path, &info) || info.is_directory) {
      return;
    }
    versions.insert(std::move(candidate));
  });
  return versions;
}

}  // namespace

ChildModuleWatcher::ChildModuleWatcher(
    base::FilePath modules_dir,
    VersionSetCallback on_version_set_changed)
    : modules_dir_(std::move(modules_dir)),
      on_version_set_changed_(std::move(on_version_set_changed)),
      debounce_timer_(FROM_HERE,
                      kDebounceDelay,
                      this,
                      &ChildModuleWatcher::Scan) {
  CHECK(on_version_set_changed_);

  if (!modules_dir_.empty()) {
    // If Watch fails, continue running so the periodic timer can still rescan.
    file_path_watcher_.Watch(
        modules_dir_, base::FilePathWatcher::Type::kRecursive,
        base::BindRepeating(&ChildModuleWatcher::OnDirectoryChanged,
                            weak_factory_.GetWeakPtr()));
    periodic_timer_.Start(FROM_HERE, kPeriodicRescanInterval, this,
                          &ChildModuleWatcher::Scan);
    Scan();
  }
}

ChildModuleWatcher::~ChildModuleWatcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ChildModuleWatcher::OnDirectoryChanged(const base::FilePath& path,
                                            bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  debounce_timer_.Reset();
}

void ChildModuleWatcher::Scan() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VersionSet new_versions = ScanChildModules(modules_dir_);
  if (new_versions != current_versions_) {
    current_versions_ = std::move(new_versions);
    on_version_set_changed_.Run(current_versions_);
  }
}

}  // namespace child_module
