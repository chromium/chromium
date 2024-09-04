// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browsing_data_remover.h"

class Profile;
class BrandcodeConfigFetcher;

namespace base {
class AtomicFlag;
}

namespace {
FORWARD_DECLARE_TEST(ProfileResetterTest, ResetNTPCustomizationsTest);
}

// This class allows resetting certain aspects of a profile to default values.
// It is used in case the profile has been damaged due to malware or bad user
// settings.
class ProfileResetter : public content::BrowsingDataRemover::Observer {
 public:
  // Flags indicating what aspects of a profile shall be reset.
  enum Resettable {
    DEFAULT_SEARCH_ENGINE = 1 << 0,
    HOMEPAGE = 1 << 1,
    CONTENT_SETTINGS = 1 << 2,
    COOKIES_AND_SITE_DATA = 1 << 3,
    EXTENSIONS = 1 << 4,
    STARTUP_PAGES = 1 << 5,
    PINNED_TABS = 1 << 6,
    SHORTCUTS = 1 << 7,
    NTP_CUSTOMIZATIONS = 1 << 8,
    LANGUAGES = 1 << 9,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    DNS_CONFIGURATIONS = 1 << 10,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // This flag should be used for ResetProfile function, if you intend to add
    // another reset to the reset profile, please edit this flag.
    PROFILE_RESETS = DEFAULT_SEARCH_ENGINE | HOMEPAGE | CONTENT_SETTINGS |
                     COOKIES_AND_SITE_DATA | EXTENSIONS | STARTUP_PAGES |
                     PINNED_TABS | SHORTCUTS | NTP_CUSTOMIZATIONS | LANGUAGES,

    // Some of the resets have to wait for other resets to be done before
    // getting reset. For example, for a proper DNS reset, all extensions need
    // to be reset before the DNS configurations are reset, because otherwise
    // there is a possibility that an extension sets DNS config between DNS
    // config reset and extension reset.
    PHASE_2_RESETS =
#if BUILDFLAG(IS_CHROMEOS_ASH)
        DNS_CONFIGURATIONS |
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        0,

    // Update ALL if you add new values and check whether the type of
    // ResettableFlags needs to be enlarged.
    ALL =
#if BUILDFLAG(IS_CHROMEOS_ASH)
        DNS_CONFIGURATIONS |
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        DEFAULT_SEARCH_ENGINE | HOMEPAGE | CONTENT_SETTINGS |
        COOKIES_AND_SITE_DATA | EXTENSIONS | STARTUP_PAGES | PINNED_TABS |
        SHORTCUTS | NTP_CUSTOMIZATIONS | LANGUAGES,

  };

  // Bit vector for Resettable enum.
  typedef uint32_t ResettableFlags;

  static_assert(sizeof(ResettableFlags) == sizeof(Resettable),
                "ResettableFlags should be the same size as Resettable");

  explicit ProfileResetter(Profile* profile);

  ProfileResetter(const ProfileResetter&) = delete;
  ProfileResetter& operator=(const ProfileResetter&) = delete;

  // Resets the settings that are marked in the resettable flags to the default
  // value, callback will be called once the reset is complete. This function
  // will also make sure the resetter is set up properly before calling |Reset|
  // to reset the flags. If |master_settings| is NULL, the default settings for
  // the current device will be loaded and used as the default value, if not the
  // specified defaults will be used for the reset values.
  virtual void ResetSettings(
      ProfileResetter::ResettableFlags resettable_flags,
      std::unique_ptr<BrandcodedDefaultSettings> master_settings,
      base::OnceClosure callback);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Call to reset a users's DNS settings.
  virtual void ResetDnsConfigurations();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  ~ProfileResetter() override;

  virtual bool IsActive() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(::ProfileResetterTest, ResetNTPCustomizationsTest);

  // Resets |resettable_flags| and calls |callback| on the UI thread on
  // completion. |default_settings| allows the caller to specify some default
  // settings. |default_settings| shouldn't be NULL.
  void ResetSettingsImpl(
      ProfileResetter::ResettableFlags resettable_flags,
      std::unique_ptr<BrandcodedDefaultSettings> master_settings,
      base::OnceClosure callback);

  // Marks |resettable| as done and triggers |callback_| if all pending jobs
  // have completed.
  void MarkAsDone(Resettable resettable);

  void ResetDefaultSearchEngine();
  void ResetHomepage();
  void ResetContentSettings();
  void ResetCookiesAndSiteData();
  void ResetExtensions();
  void ResetStartupPages();
  void ResetPinnedTabs();
  void ResetShortcuts();
  void ResetNtpCustomizations();
  void ResetLanguages();

  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  // Callback for when TemplateURLService has loaded.
  void OnTemplateURLServiceLoaded();

  // Callback to check if the settings is fetched properly.
  void OnDefaultSettingsFetched();

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  std::unique_ptr<BrandcodedDefaultSettings> master_settings_;
  raw_ptr<TemplateURLService, DanglingUntriaged> template_url_service_;

  // Flags of a Resetable indicating which reset operations we are still waiting
  // for.
  ResettableFlags pending_reset_flags_;

  // Called on UI thread when reset has been completed.
  base::OnceClosure callback_;

  // Set when the phase 2 resets have started in order to make sure we
  // only start these resets once.
  bool phase_2_resets_started_ = false;

  // If non-null it means removal is in progress. BrowsingDataRemover takes care
  // of deleting itself when done.
  raw_ptr<content::BrowsingDataRemover> cookies_remover_;

  base::CallbackListSubscription template_url_service_subscription_;

  // Contains Chrome brand code; empty for organic Chrome.
  std::string brandcode_;

  std::unique_ptr<BrandcodeConfigFetcher> config_fetcher_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ProfileResetter> weak_ptr_factory_{this};
};

// Path to shortcut and command line arguments.
typedef std::pair<base::FilePath, std::wstring> ShortcutCommand;

typedef base::RefCountedData<base::AtomicFlag> SharedCancellationFlag;

#if BUILDFLAG(IS_WIN)
// On Windows returns all the shortcuts which launch Chrome and corresponding
// arguments. |cancel| can be passed to abort the operation earlier.
// Call on COM task runner that may block.
std::vector<ShortcutCommand> GetChromeLaunchShortcuts(
    const scoped_refptr<SharedCancellationFlag>& cancel);
#endif

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESETTER_H_
