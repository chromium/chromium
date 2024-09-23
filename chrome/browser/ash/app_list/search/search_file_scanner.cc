// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_file_scanner.h"

#include <map>
#include <string_view>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "ui/base/metadata/base_type_conversion.h"

namespace app_list {

namespace {

// The minimum time in days required to wait after the previous file scan
// logging. Assuming that the user local file won't change dramatically, log at
// most once per week to minimize the power and performance impact.
constexpr int kMinFileScanDelayDays = 7;

// The delay before we start the file scan when the file is constructed.
constexpr base::TimeDelta kFileScanDelay = base::Minutes(10);

// The key for total file numbers in `extension_file_counts`.
constexpr std::string_view kTotal = "Total";

// The list of file extensions we are interested in.
// It is in the form of {"extension format", "extension name"}, with the format
// to match the file extension and the name to get the uma metric name.
// Note:
//    When adding new extension to this map, also updates the
//    `SearchFileExtension` variants in
//    `tools/metrics/histograms/metadata/apps/histograms.xml`.
constexpr auto kExtensionMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>({
        // Image extensions.
        {".png", "Png"},
        {".jpg", "Jpg"},
        {".jpeg", "Jpeg"},
        {".webp", "Webp"},
    });

base::Time GetLastScanLogTime(Profile* profile) {
  return profile->GetPrefs()->GetTime(
      ash::prefs::kLauncherSearchLastFileScanLogTime);
}

void SetLastScanLogTime(Profile* profile, const base::Time& time) {
  profile->GetPrefs()->SetTime(ash::prefs::kLauncherSearchLastFileScanLogTime,
                               time);
}

// Looks up and counts the total file number and the file numbers of each
// extension that is of interest in the `search_path` and excludes the ones that
// overlaps with `trash_paths`.
//
// This function can be executed on any thread other than the UI thread, while
// the `SearchFileScanner` is created on UI thread.
void FullScan(const base::FilePath& search_path,
              const std::vector<base::FilePath>& trash_paths) {
  base::Time start_time = base::Time::Now();
  base::FileEnumerator file_enumerator(search_path, /*recursive=*/true,
                                       base::FileEnumerator::FILES);

  std::map<std::string_view, int> extension_file_counts;
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // Exclude any paths that are parented at an enabled trash location.
    if (base::ranges::any_of(trash_paths,
                             [&file_path](const base::FilePath& trash_path) {
                               return trash_path.IsParent(file_path);
                             })) {
      continue;
    }
    // Always counts the total file number.
    extension_file_counts[kTotal]++;

    // Increments the extension count if the extension of the current file is of
    // interest.
    const auto extension_lookup =
        kExtensionMap.find(base::ToLowerASCII(file_path.Extension()));
    if (extension_lookup != kExtensionMap.end()) {
      extension_file_counts[extension_lookup->second]++;
    }
  }

  // Logs execution time.
  base::UmaHistogramTimes("Apps.AppList.SearchFileScan.ExecutionTime",
                          base::Time::Now() - start_time);

  // Logs file extension count data.
  for (const auto& it : extension_file_counts) {
    base::UmaHistogramCounts100000(
        base::StrCat({"Apps.AppList.SearchFileScan.", it.first}), it.second);
  }
}

}  // namespace

SearchFileScanner::SearchFileScanner(
    Profile* profile,
    const base::FilePath& root_path,
    const std::vector<base::FilePath>& excluded_paths,
    std::optional<base::TimeDelta> start_delay_override)
    : profile_(profile),
      root_path_(root_path),
      excluded_paths_(excluded_paths) {
  // Early returns if file scan has been done recently.
  // TODO(b/337130427) considering replacing this with a timer, which can covers
  // the users which does not logout frequently.
  if (profile_ && base::Time::Now() - GetLastScanLogTime(profile_) <
                      base::Days(kMinFileScanDelayDays)) {
    return;
  }

  // Delays the file scan so that it does not add regressions to the user login.
  // Skips the delay in test.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SearchFileScanner::StartFileScan,
                     weak_factory_.GetWeakPtr()),
      start_delay_override.value_or(kFileScanDelay));
}

SearchFileScanner::~SearchFileScanner() = default;

void SearchFileScanner::StartFileScan() {
  // Do the file scan on a non-UI thread.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FullScan, std::move(root_path_),
                     std::move(excluded_paths_)),
      base::BindOnce(&SearchFileScanner::OnScanComplete,
                     weak_factory_.GetWeakPtr()));
}

void SearchFileScanner::OnScanComplete() {
  // Ensures the `profile_` is still alive to avoid any possible crashes during
  // shutdown.
  if (profile_) {
    SetLastScanLogTime(profile_, base::Time::Now());
  }
}

}  // namespace app_list
