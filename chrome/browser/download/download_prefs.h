// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_

#include <memory>
#include <set>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"

class GURL;
class Profile;
class TrustedSourcesManager;

namespace content {
class BrowserContext;
class DownloadManager;
}

namespace download {
class DownloadItem;
}

namespace policy {
class URLBlocklist;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Stores all download-related preferences.
class DownloadPrefs {
 public:
  enum class DownloadRestriction {
    NONE = 0,
    DANGEROUS_FILES = 1,
    POTENTIALLY_DANGEROUS_FILES = 2,
    ALL_FILES = 3,
    // MALICIOUS_FILES has a stricter definition of harmful file than
    // DANGEROUS_FILES and does not block based on file extension.
    MALICIOUS_FILES = 4,
  };
  explicit DownloadPrefs(Profile* profile);

  DownloadPrefs(const DownloadPrefs&) = delete;
  DownloadPrefs& operator=(const DownloadPrefs&) = delete;

  ~DownloadPrefs();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default download directory.
  static const base::FilePath& GetDefaultDownloadDirectory();

  // Returns the default download directory for the current profile.
  base::FilePath GetDefaultDownloadDirectoryForProfile() const;

  // Returns the DownloadPrefs corresponding to the given DownloadManager
  // or BrowserContext.
  static DownloadPrefs* FromDownloadManager(
      content::DownloadManager* download_manager);
  static DownloadPrefs* FromBrowserContext(
      content::BrowserContext* browser_context);

  // Identify whether the downloaded item was downloaded from a trusted source.
  bool IsFromTrustedSource(const download::DownloadItem& item);

  base::FilePath DownloadPath() const;
  void SetDownloadPath(const base::FilePath& path);
  base::FilePath SaveFilePath() const;
  void SetSaveFilePath(const base::FilePath& path);
  int save_file_type() const { return *save_file_type_; }
  void SetSaveFileType(int type);
  DownloadRestriction download_restriction() const {
    return static_cast<DownloadRestriction>(*download_restriction_);
  }
  bool safebrowsing_for_trusted_sources_enabled() const {
    return *safebrowsing_for_trusted_sources_enabled_;
  }

  // Returns true if the prompt_for_download preference has been set and the
  // download location is not managed (which means the user shouldn't be able
  // to choose another download location).
  bool PromptForDownload() const;

  // Returns true if the download path preference is managed.
  bool IsDownloadPathManaged() const;

  // Returns true if there is at least one file extension registered
  // by the user for auto-open.
  bool IsAutoOpenByUserUsed() const;

  // Returns true if |path| should be opened automatically.
  bool IsAutoOpenEnabled(const GURL& url, const base::FilePath& path) const;

  // Returns true if |path| should be opened automatically by policy.
  bool IsAutoOpenByPolicy(const GURL& url, const base::FilePath& path) const;

  // Enables automatically opening all downloads with the same file type as
  // |file_name|. Returns true on success. The call may fail if |file_name|
  // either doesn't have an extension (hence the file type cannot be
  // determined), or if the file type is one that is disallowed from being
  // opened automatically. See IsAllowedToOpenAutomatically() for details on the
  // latter.
  bool EnableAutoOpenByUserBasedOnExtension(const base::FilePath& file_name);

  // Disables auto-open based on file extension.
  void DisableAutoOpenByUserBasedOnExtension(const base::FilePath& file_name);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  // Store the user preference to disk. If |should_open| is true, also disable
  // the built-in PDF plugin. If |should_open| is false, enable the PDF plugin.
  void SetShouldOpenPdfInSystemReader(bool should_open);

  // Return whether the user prefers to open PDF downloads in the platform's
  // default reader.
  bool ShouldOpenPdfInSystemReader() const;
#endif

  void ResetAutoOpenByUser();

  // If this is called, the download target path will not be sanitized going
  // forward - whatever has been passed to SetDownloadPath will be used.
  void SkipSanitizeDownloadTargetPathForTesting();

#if BUILDFLAG(IS_ANDROID)
  // Returns whether downloaded pdf from external apps should be auto-opened.
  bool IsAutoOpenPdfEnabled();
#endif
 private:
  void SaveAutoOpenState();
  bool CanPlatformEnableAutoOpenForPdf() const;

  // Checks whether |path| is a valid download target path. If it is, returns
  // it as is. If it isn't returns the default download directory.
  base::FilePath SanitizeDownloadTargetPath(const base::FilePath& path) const;

  void UpdateAutoOpenByPolicy();

  void UpdateAllowedURLsForOpenByPolicy();

  raw_ptr<Profile> profile_;

  BooleanPrefMember prompt_for_download_;
#if BUILDFLAG(IS_ANDROID)
  IntegerPrefMember prompt_for_download_android_;
  BooleanPrefMember auto_open_pdf_enabled_;
#endif

  FilePathPrefMember download_path_;
  FilePathPrefMember save_file_path_;
  IntegerPrefMember save_file_type_;
  IntegerPrefMember download_restriction_;
  BooleanPrefMember safebrowsing_for_trusted_sources_enabled_;

  PrefChangeRegistrar pref_change_registrar_;

  // To identify if a download URL is from a trusted source.
  std::unique_ptr<TrustedSourcesManager> trusted_sources_manager_;

  // Set of file extensions to open at download completion.
  struct AutoOpenCompareFunctor {
    bool operator()(const base::FilePath::StringType& a,
                    const base::FilePath::StringType& b) const;
  };
  typedef std::set<base::FilePath::StringType,
                   AutoOpenCompareFunctor> AutoOpenSet;
  AutoOpenSet auto_open_by_user_;
  AutoOpenSet auto_open_by_policy_;

  std::unique_ptr<policy::URLBlocklist> auto_open_allowed_by_urls_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
  bool should_open_pdf_in_system_reader_;
#endif

  // If this is true, SanitizeDownloadTargetPath will always return the passed
  // path verbatim.
  bool skip_sanitize_download_target_path_for_testing_ = false;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
