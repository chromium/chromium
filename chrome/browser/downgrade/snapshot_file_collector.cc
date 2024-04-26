// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_file_collector.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/common/chrome_constants.h"
#include "components/affiliations/core/browser/affiliation_constants.h"
#include "components/autofill/core/browser/strike_databases/strike_database.h"
#include "components/bookmarks/common/bookmark_constants.h"
#include "components/history/core/browser/history_constants.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/sessions/core/session_constants.h"
#include "components/webdata/common/webdata_constants.h"
#include "content/public/browser/browsing_data_remover.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/profiles/profile_shortcut_manager_win.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/last_browser_file_util.h"
#endif

namespace downgrade {

SnapshotItemDetails::SnapshotItemDetails(base::FilePath path,
                                         ItemType item_type,
                                         uint64_t data_types,
                                         SnapshotItemId id)
    : path(std::move(path)),
      is_directory(item_type == ItemType::kDirectory),
      data_types(data_types),
      id(id) {}

// Returns a list of items to snapshot that should be directly under the user
// data  directory.
std::vector<SnapshotItemDetails> CollectUserDataItems() {
  std::vector<SnapshotItemDetails> user_data_items{
      SnapshotItemDetails(base::FilePath(chrome::kLocalStateFilename),
                          SnapshotItemDetails::ItemType::kFile, 0,
                          SnapshotItemId::kLocalState),
      SnapshotItemDetails(base::FilePath(profiles::kHighResAvatarFolderName),
                          SnapshotItemDetails::ItemType::kDirectory, 0,
                          SnapshotItemId::kHighResAvatar)};
#if BUILDFLAG(IS_WIN)
  user_data_items.emplace_back(base::FilePath(web_app::kLastBrowserFilename),
                               SnapshotItemDetails::ItemType::kFile, 0,
                               SnapshotItemId::kLastBrowser);
#endif  // BUILDFLAG(IS_WIN)
  return user_data_items;
}

// Returns a list of items to snapshot that should be under a profile directory.
std::vector<SnapshotItemDetails> CollectProfileItems() {
  // Data mask to delete the pref files if any of the following types is
  // deleted. When cookies are deleted, the kZeroSuggestCachedResults and
  // kZeroSuggestCachedResultsWithURL prefs have to be reset. When history and
  // isolated origins are deleted, the kPrefLastLaunchTime and
  // kUserTriggeredIsolatedOrigins prefs have to be reset. When data type
  // content is deleted, blocklisted sites are deleted from the translation
  // prefs.
  uint64_t pref_data_type =
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      chrome_browsing_data_remover::DATA_TYPE_ISOLATED_ORIGINS |
      chrome_browsing_data_remover::DATA_TYPE_HISTORY |
      chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS;
  std::vector<SnapshotItemDetails> profile_items{
      // General Profile files
      SnapshotItemDetails(base::FilePath(chrome::kPreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile, pref_data_type,
                          SnapshotItemId::kPreferences),
      SnapshotItemDetails(base::FilePath(chrome::kSecurePreferencesFilename),
                          SnapshotItemDetails::ItemType::kFile, pref_data_type,
                          SnapshotItemId::kSecurePreferences),
      // History files
      SnapshotItemDetails(base::FilePath(history::kHistoryFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
                          SnapshotItemId::kHistory),
      SnapshotItemDetails(base::FilePath(history::kFaviconsFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
                          SnapshotItemId::kFavicons),
      SnapshotItemDetails(base::FilePath(history::kTopSitesFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
                          SnapshotItemId::kTopSites),
      // Bookmarks
      SnapshotItemDetails(
          base::FilePath(bookmarks::kLocalOrSyncableBookmarksFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_BOOKMARKS,
          SnapshotItemId::kLocalOrSyncableBookmarks),
      SnapshotItemDetails(base::FilePath(bookmarks::kAccountBookmarksFileName),
                          SnapshotItemDetails::ItemType::kFile,
                          chrome_browsing_data_remover::DATA_TYPE_BOOKMARKS,
                          SnapshotItemId::kAccountBookmarks),
      // Tab Restore and sessions
      // TODO(crbug.com/40704630): Remove legacy snapshots in M89
      SnapshotItemDetails(
          base::FilePath(sessions::kLegacyCurrentTabSessionFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
          SnapshotItemId::kLegacyCurrentTabSession),
      SnapshotItemDetails(
          base::FilePath(sessions::kLegacyCurrentSessionFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
          SnapshotItemId::kLegacyCurrentSession),
      SnapshotItemDetails(base::FilePath(sessions::kSessionsDirectory),
                          SnapshotItemDetails::ItemType::kDirectory,
                          chrome_browsing_data_remover::DATA_TYPE_HISTORY,
                          SnapshotItemId::kSessions),
      // Sign-in state
      SnapshotItemDetails(base::FilePath(profiles::kGAIAPictureFileName),
                          SnapshotItemDetails::ItemType::kFile, 0,
                          SnapshotItemId::kGAIAPicture),
      // Password / Autofill
      SnapshotItemDetails(
          base::FilePath(affiliations::kAffiliationDatabaseFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
              chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
          SnapshotItemId::kAffiliationDatabase),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForProfileFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
              chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
          SnapshotItemId::kLoginDataForProfile),
      SnapshotItemDetails(
          base::FilePath(password_manager::kLoginDataForAccountFileName),
          SnapshotItemDetails::ItemType::kFile,
          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
              chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
          SnapshotItemId::kLoginDataForAccount),
      SnapshotItemDetails(base::FilePath(kWebDataFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
                              chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
                          SnapshotItemId::kWebData),
      SnapshotItemDetails(base::FilePath(autofill::kStrikeDatabaseFileName),
                          SnapshotItemDetails::ItemType::kDirectory,
                          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
                              chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
                          SnapshotItemId::kStrikeDatabase),
      // Cookies
      SnapshotItemDetails(base::FilePath(chrome::kCookieFilename),
                          SnapshotItemDetails::ItemType::kFile,
                          content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                          SnapshotItemId::kCookie)};

#if BUILDFLAG(IS_WIN)
  // Sign-in state
  profile_items.emplace_back(base::FilePath(profiles::kProfileIconFileName),
                             SnapshotItemDetails::ItemType::kFile, 0,
                             SnapshotItemId::kProfileIcon);
#endif  // BUILDFLAG(IS_WIN)
  return profile_items;
}

}  // namespace downgrade
