// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"

#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/win/registry.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

// Returns the post-cleanup reset pending prefs for |profile|.
bool ResetPending(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(PostCleanupSettingsResetter::IsEnabled());
  DCHECK(profile);

  PrefService* prefs = profile->GetPrefs();
  return prefs->GetBoolean(prefs::kChromeCleanerResetPending);
}

// Updates the post-cleanup reset pending prefs for |profile|.
void RecordResetPending(bool value, Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(PostCleanupSettingsResetter::IsEnabled());
  DCHECK(profile);

  PrefService* prefs = profile->GetPrefs();
  prefs->SetBoolean(prefs::kChromeCleanerResetPending, value);
}

bool CopyProfilesToReset(const std::vector<Profile*>& profiles,
                         std::vector<Profile*>* profiles_to_reset) {
  base::ranges::copy_if(profiles, std::back_inserter(*profiles_to_reset),
                        &ResetPending);
  return !profiles_to_reset->empty();
}

// Manages post-cleanup settings reset for a list of profiles.
// An instance of this class is created by ResetTaggedProfiles() and will
// self-delete once all profiles in the list have been reset.
class SettingsResetter : public base::RefCounted<SettingsResetter> {
 public:
  SettingsResetter(
      std::vector<Profile*> profiles_to_reset,
      std::unique_ptr<PostCleanupSettingsResetter::Delegate> delegate,
      base::OnceClosure done_callback);

  SettingsResetter(const SettingsResetter&) = delete;
  SettingsResetter& operator=(const SettingsResetter&) = delete;

  // Resets settings for all profiles in |profiles_to_reset_| and invokes
  // |done_callback_| when done.
  void Run();

 protected:
  virtual ~SettingsResetter();

 private:
  friend class base::RefCounted<SettingsResetter>;

  // Resets settings for |profile| according to default values given by
  // |main_settings|. Used as a callback for
  // DefaultSettingsFetcher::FetchDefaultSettings().
  void OnFetchCompleted(
      Profile* profile,
      std::unique_ptr<BrandcodedDefaultSettings> main_settings);

  // Removes the settings reset tag for |profile|. If there are no more
  // profiles to reset, invokes |done_callback_| and deletes this object.
  void OnResetCompleted(Profile* profile);

  // The profiles to be reset.
  std::vector<Profile*> profiles_to_reset_;

  // The ProfileResetter objects that are used to reset each profile. We need to
  // hold on to these until each reset operation has been completed.
  std::vector<std::unique_ptr<ProfileResetter>> profile_resetters_;

  // Used to check that modifications to |profile_resetters_| are sequenced
  // correctly.
  SEQUENCE_CHECKER(sequence_checker_);

  // The number of profiles that need to be reset.
  int num_pending_resets_;

  // The callback to be invoked once settings reset completes.
  base::OnceClosure done_callback_;

  std::unique_ptr<PostCleanupSettingsResetter::Delegate> delegate_;
};

SettingsResetter::SettingsResetter(
    std::vector<Profile*> profiles_to_reset,
    std::unique_ptr<PostCleanupSettingsResetter::Delegate> delegate,
    base::OnceClosure done_callback)
    : profiles_to_reset_(std::move(profiles_to_reset)),
      num_pending_resets_(profiles_to_reset_.size()),
      done_callback_(std::move(done_callback)),
      delegate_(std::move(delegate)) {
  DCHECK_LT(0, num_pending_resets_);
  DCHECK(done_callback_);
  DCHECK(delegate_);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SettingsResetter::~SettingsResetter() {
  DCHECK(!done_callback_);
  DCHECK(!num_pending_resets_);
}

void SettingsResetter::Run() {
  for (Profile* profile : profiles_to_reset_) {
    delegate_->FetchDefaultSettings(
        base::BindOnce(&SettingsResetter::OnFetchCompleted, this, profile));
  }
}

void SettingsResetter::OnFetchCompleted(
    Profile* profile,
    std::unique_ptr<BrandcodedDefaultSettings> main_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static const ProfileResetter::ResettableFlags kSettingsToReset =
      ProfileResetter::DEFAULT_SEARCH_ENGINE | ProfileResetter::HOMEPAGE |
      ProfileResetter::EXTENSIONS | ProfileResetter::STARTUP_PAGES |
      ProfileResetter::SHORTCUTS;

  profile_resetters_.push_back(delegate_->GetProfileResetter(profile));
  profile_resetters_.back()->Reset(
      kSettingsToReset, std::move(main_settings),
      base::BindOnce(&SettingsResetter::OnResetCompleted, this, profile));
}

void SettingsResetter::OnResetCompleted(Profile* profile) {
  DCHECK_LT(0, num_pending_resets_);

  RecordResetPending(false, profile);

  --num_pending_resets_;
  if (!num_pending_resets_)
    std::move(done_callback_).Run();
}

// Returns true if there is information of a completed cleanup in the registry.
bool CleanupCompletedFromRegistry() {
  std::wstring cleaner_key_path(
      chrome_cleaner::kSoftwareRemovalToolRegistryKey);
  cleaner_key_path.append(L"\\").append(chrome_cleaner::kCleanerSubKey);

  base::win::RegKey srt_cleaner_key(HKEY_CURRENT_USER, cleaner_key_path.c_str(),
                                    KEY_QUERY_VALUE);
  DWORD cleanup_completed = 0;
  return srt_cleaner_key.Valid() &&
         srt_cleaner_key.ReadValueDW(chrome_cleaner::kCleanupCompletedValueName,
                                     &cleanup_completed) == ERROR_SUCCESS &&
         cleanup_completed == 1;
}

}  // namespace

PostCleanupSettingsResetter::Delegate::Delegate() {}

PostCleanupSettingsResetter::Delegate::~Delegate() {}

void PostCleanupSettingsResetter::Delegate::FetchDefaultSettings(
    DefaultSettingsFetcher::SettingsCallback callback) {
  DefaultSettingsFetcher::FetchDefaultSettings(std::move(callback));
}

PostCleanupSettingsResetter::PostCleanupSettingsResetter() = default;

PostCleanupSettingsResetter::~PostCleanupSettingsResetter() = default;

// static
bool PostCleanupSettingsResetter::IsEnabled() {
#if BUILDFLAG(IS_WIN)
  return true;
#else
  return false;
#endif
}

std::unique_ptr<ProfileResetter>
PostCleanupSettingsResetter::Delegate::GetProfileResetter(Profile* profile) {
  return std::make_unique<ProfileResetter>(profile);
}

void PostCleanupSettingsResetter::TagForResetting(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsEnabled());
  DCHECK(profile);

  RecordResetPending(true, profile);
}

void PostCleanupSettingsResetter::ResetTaggedProfiles(
    std::vector<Profile*> profiles,
    base::OnceClosure done_callback,
    std::unique_ptr<PostCleanupSettingsResetter::Delegate> delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(IsEnabled());
  DCHECK(delegate);

  std::vector<Profile*> profiles_to_reset;
  if (!CopyProfilesToReset(profiles, &profiles_to_reset) ||
      !CleanupCompletedFromRegistry()) {
    std::move(done_callback).Run();
    return;
  }

  UMA_HISTOGRAM_EXACT_LINEAR("SoftwareReporter.PostCleanupSettingsReset",
                             profiles_to_reset.size(), 10);

  // The SettingsResetter object will self-delete once |done_callback| is
  // invoked.
  base::WrapRefCounted(new SettingsResetter(std::move(profiles_to_reset),
                                            std::move(delegate),
                                            std::move(done_callback)))
      ->Run();
}

// static
void PostCleanupSettingsResetter::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  DCHECK(registry);
  registry->RegisterBooleanPref(prefs::kChromeCleanerResetPending, false);
}

}  // namespace safe_browsing
