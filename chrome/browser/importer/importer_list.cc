// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#include "chrome/common/importer/safari_importer_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
void DetectIEProfiles(std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // IE always exists and doesn't have multiple profiles.
  importer::SourceProfile ie;
  ie.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie.importer_type = importer::TYPE_IE;
  ie.services_supported =
      importer::HISTORY | importer::FAVORITES | importer::SEARCH_ENGINES;
  profiles->push_back(ie);
}

void DetectEdgeProfiles(std::vector<importer::SourceProfile>* profiles) {
  if (!importer::EdgeImporterCanImport())
    return;
  importer::SourceProfile edge;
  edge.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_EDGE);
  edge.importer_type = importer::TYPE_EDGE;
  edge.services_supported = importer::FAVORITES;
  edge.source_path = importer::GetEdgeDataFilePath();
  profiles->push_back(edge);
}

void DetectBuiltinWindowsProfiles(
    std::vector<importer::SourceProfile>* profiles) {
  if (shell_integration::IsIEDefaultBrowser()) {
    DetectIEProfiles(profiles);
    DetectEdgeProfiles(profiles);
  } else {
    DetectEdgeProfiles(profiles);
    DetectIEProfiles(profiles);
  }
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
void DetectSafariProfiles(std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  uint16_t items = importer::NONE;
  if (!SafariImporterCanImport(base::apple::GetUserLibraryPath(), &items)) {
    return;
  }

  importer::SourceProfile safari;
  safari.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari.importer_type = importer::TYPE_SAFARI;
  safari.services_supported = items;
  profiles->push_back(safari);
}
#endif  // BUILDFLAG(IS_MAC)

// |locale|: The application locale used for lookups in Firefox's
// locale-specific search engines feature (see firefox_importer.cc for
// details).
void DetectFirefoxProfiles(const std::string locale,
                           std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
#if BUILDFLAG(IS_WIN)
  const std::string firefox_install_id =
      shell_integration::GetFirefoxProgIdSuffix();
#else
  const std::string firefox_install_id;
#endif  // BUILDFLAG(IS_WIN)
  std::vector<FirefoxDetail> details = GetFirefoxDetails(firefox_install_id);
  if (details.empty())
    return;

  for (const auto& detail : details) {
    base::FilePath app_path;
    if (detail.path.empty())
      continue;

    int version = 0;
#if BUILDFLAG(IS_WIN)
    version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif

    if (version < 2) {
      GetFirefoxVersionAndPathFromProfile(detail.path, &version, &app_path);
      // Note that |version| is re-assigned above.
      if (version < 48) {
        // While this cutoff (and the removal of outdated code) should depend on
        // the usage percent of different versions of Firefox (see
        // https://en.wikipedia.org/wiki/Firefox_version_history), the reality
        // is that the current cutoff of at least Firefox 48 is mostly due to
        // the fact that there's a Firefox 48 profile for testing in
        // firefox_importer_unittest.cc. TODO(crbug.com/40169760): Add
        // more modern Firefox test profiles, and roll the cutoff version.
        continue;
      }
    }

    importer::SourceProfile firefox;
    firefox.importer_name = GetFirefoxImporterName(app_path);
    firefox.profile = detail.name;
    firefox.importer_type = importer::TYPE_FIREFOX;
    firefox.source_path = detail.path;
#if BUILDFLAG(IS_WIN)
    firefox.app_path = GetFirefoxInstallPathFromRegistry();
#endif
    if (firefox.app_path.empty())
      firefox.app_path = app_path;
    firefox.services_supported =
        importer::HISTORY | importer::FAVORITES | importer::AUTOFILL_FORM_DATA;
#if !BUILDFLAG(IS_MAC)
    // Passwords are imported by loading the NSS DLLs into the Chromium process.
    // Restrictive code signing prevents that from ever working again in modern
    // macOSes, so don't promise an import service that can't be delivered.
    firefox.services_supported |= importer::PASSWORDS;
#endif
    firefox.locale = locale;
    profiles->push_back(firefox);
  }
}

std::vector<importer::SourceProfile> DetectSourceProfilesWorker(
    const std::string& locale,
    bool include_interactive_profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<importer::SourceProfile> profiles;

  // The first run import will automatically take settings from the first
  // profile detected, which should be the user's current default.
#if BUILDFLAG(IS_WIN)
  if (shell_integration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectBuiltinWindowsProfiles(&profiles);
  } else {
    DetectBuiltinWindowsProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#elif BUILDFLAG(IS_MAC)
  if (shell_integration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectSafariProfiles(&profiles);
  } else {
    DetectSafariProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#else
  DetectFirefoxProfiles(locale, &profiles);
#endif
  if (include_interactive_profiles) {
    importer::SourceProfile bookmarks_profile;
    bookmarks_profile.importer_name =
        l10n_util::GetStringUTF16(IDS_IMPORT_FROM_BOOKMARKS_HTML_FILE);
    bookmarks_profile.importer_type = importer::TYPE_BOOKMARKS_FILE;
    bookmarks_profile.services_supported = importer::FAVORITES;
    profiles.push_back(bookmarks_profile);
  }

  return profiles;
}

}  // namespace

ImporterList::ImporterList() = default;

ImporterList::~ImporterList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ImporterList::DetectSourceProfiles(
    const std::string& locale,
    bool include_interactive_profiles,
    base::OnceClosure profiles_loaded_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DetectSourceProfilesWorker, locale,
                     include_interactive_profiles),
      base::BindOnce(&ImporterList::SourceProfilesLoaded,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(profiles_loaded_callback)));
}

const importer::SourceProfile& ImporterList::GetSourceProfileAt(
    size_t index) const {
  DCHECK_LT(index, count());
  return source_profiles_[index];
}

void ImporterList::SourceProfilesLoaded(
    base::OnceClosure profiles_loaded_callback,
    const std::vector<importer::SourceProfile>& profiles) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_profiles_.assign(profiles.begin(), profiles.end());
  std::move(profiles_loaded_callback).Run();
}
