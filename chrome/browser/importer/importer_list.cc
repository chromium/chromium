// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/firefox_importer_utils.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/foundation_util.h"
#include "chrome/common/importer/safari_importer_utils.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/importer/edge_importer_utils_win.h"
#endif

namespace {

#if defined(OS_WIN)
void DetectIEProfiles(std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // IE always exists and doesn't have multiple profiles.
  importer::SourceProfile ie;
  ie.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie.importer_type = importer::TYPE_IE;
  ie.services_supported = importer::HISTORY | importer::FAVORITES |
                          importer::COOKIES | importer::PASSWORDS |
                          importer::SEARCH_ENGINES;
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

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void DetectSafariProfiles(std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  uint16_t items = importer::NONE;
  if (!SafariImporterCanImport(base::mac::GetUserLibraryPath(), &items))
    return;

  importer::SourceProfile safari;
  safari.importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari.importer_type = importer::TYPE_SAFARI;
  safari.services_supported = items;
  profiles->push_back(safari);
}
#endif  // defined(OS_MACOSX)

// |locale|: The application locale used for lookups in Firefox's
// locale-specific search engines feature (see firefox_importer.cc for
// details).
void DetectFirefoxProfiles(const std::string locale,
                           std::vector<importer::SourceProfile>* profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
#if defined(OS_WIN)
  const std::string firefox_install_id =
      shell_integration::GetFirefoxProgIdSuffix();
#else
  const std::string firefox_install_id;
#endif  // defined(OS_WIN)
  base::FilePath profile_path = GetFirefoxProfilePath(firefox_install_id);
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ImporterType firefox_type;
  base::FilePath app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version < 2)
    GetFirefoxVersionAndPathFromProfile(profile_path, &version, &app_path);

  if (version >= 3) {
    firefox_type = importer::TYPE_FIREFOX;
  } else {
    // Ignores old versions of firefox.
    return;
  }

  importer::SourceProfile firefox;
  firefox.importer_name = GetFirefoxImporterName(app_path);
  firefox.importer_type = firefox_type;
  firefox.source_path = profile_path;
#if defined(OS_WIN)
  firefox.app_path = GetFirefoxInstallPathFromRegistry();
#endif
  if (firefox.app_path.empty())
    firefox.app_path = app_path;
  firefox.services_supported = importer::HISTORY | importer::FAVORITES |
                               importer::PASSWORDS | importer::SEARCH_ENGINES |
                               importer::AUTOFILL_FORM_DATA;
  firefox.locale = locale;
  profiles->push_back(firefox);
}

std::vector<importer::SourceProfile> DetectSourceProfilesWorker(
    const std::string& locale,
    bool include_interactive_profiles) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::vector<importer::SourceProfile> profiles;

  // The first run import will automatically take settings from the first
  // profile detected, which should be the user's current default.
#if defined(OS_WIN)
  if (shell_integration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(locale, &profiles);
    DetectBuiltinWindowsProfiles(&profiles);
  } else {
    DetectBuiltinWindowsProfiles(&profiles);
    DetectFirefoxProfiles(locale, &profiles);
  }
#elif defined(OS_MACOSX)
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

ImporterList::ImporterList() {}

ImporterList::~ImporterList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ImporterList::DetectSourceProfiles(
    const std::string& locale,
    bool include_interactive_profiles,
    const base::Closure& profiles_loaded_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::Bind(&DetectSourceProfilesWorker, locale,
                 include_interactive_profiles),
      base::Bind(&ImporterList::SourceProfilesLoaded,
                 weak_ptr_factory_.GetWeakPtr(), profiles_loaded_callback));
}

const importer::SourceProfile& ImporterList::GetSourceProfileAt(
    size_t index) const {
  DCHECK_LT(index, count());
  return source_profiles_[index];
}

void ImporterList::SourceProfilesLoaded(
    const base::Closure& profiles_loaded_callback,
    const std::vector<importer::SourceProfile>& profiles) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  source_profiles_.assign(profiles.begin(), profiles.end());
  profiles_loaded_callback.Run();
}
