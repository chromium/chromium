// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/avatar_menu_actions.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_list.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using content::BrowserThread;

AvatarMenu::AvatarMenu(ProfileAttributesStorage* profile_storage,
                       AvatarMenuObserver* observer,
                       Browser* browser)
    : profile_list_(ProfileList::Create(profile_storage)),
      menu_actions_(AvatarMenuActions::Create()),
      profile_storage_(profile_storage->AsWeakPtr()),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_storage_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  ActiveBrowserChanged(browser_);

  // Register this as an observer of the info cache.
  profile_storage_->AddObserver(this);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // Register this as an observer of the SupervisedUserService to be notified
  // of changes to the custodian info.
  if (browser_) {
    supervised_user_observer_.Add(
        SupervisedUserServiceFactory::GetForProfile(browser_->profile()));
  }
#endif
}

AvatarMenu::~AvatarMenu() {
  // Note that |profile_storage_| may be destroyed before |this|.
  // https://crbug.com/1008947
  if (profile_storage_)
    profile_storage_->RemoveObserver(this);
}

AvatarMenu::Item::Item(size_t menu_index, const base::FilePath& profile_path,
                       const gfx::Image& icon)
    : icon(icon),
      active(false),
      signed_in(false),
      signin_required(false),
      menu_index(menu_index),
      profile_path(profile_path) {
}

AvatarMenu::Item::Item(const Item& other) = default;

AvatarMenu::Item::~Item() {
}

void AvatarMenu::SwitchToProfile(size_t index,
                                 bool always_create,
                                 ProfileMetrics::ProfileOpen metric) {
  DCHECK(profiles::IsMultipleProfilesEnabled() ||
         index == GetActiveProfileIndex());
  const Item& item = GetItemAt(index);

  // Don't open a browser window for signed-out profiles.
  if (item.signin_required) {
    UserManager::Show(item.profile_path,
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
    return;
  }

  profiles::SwitchToProfile(item.profile_path, always_create,
                            ProfileManager::CreateCallback(), metric);
}

void AvatarMenu::AddNewProfile(ProfileMetrics::ProfileAdd type) {
  menu_actions_->AddNewProfile(type);
}

void AvatarMenu::EditProfile(size_t index) {
  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_list_->GetItemAt(index).profile_path);

  menu_actions_->EditProfile(profile);
}

void AvatarMenu::RebuildMenu() {
  profile_list_->RebuildMenu();
}

size_t AvatarMenu::GetNumberOfItems() const {
  return profile_list_->GetNumberOfItems();
}

const AvatarMenu::Item& AvatarMenu::GetItemAt(size_t index) const {
  return profile_list_->GetItemAt(index);
}

size_t AvatarMenu::GetIndexOfItemWithProfilePath(const base::FilePath& path) {
  return profile_list_->MenuIndexFromProfilePath(path);
}

size_t AvatarMenu::GetActiveProfileIndex() {
  // During singleton profile deletion, this function can be called with no
  // profiles in the model - crbug.com/102278 .
  if (profile_list_->GetNumberOfItems() == 0)
    return 0;

  Profile* active_profile = browser_ ? browser_->profile()
                                     : ProfileManager::GetLastUsedProfile();

  size_t index =
      profile_list_->MenuIndexFromProfilePath(active_profile->GetPath());
  DCHECK_LT(index, profile_list_->GetNumberOfItems());
  return index;
}

base::string16 AvatarMenu::GetSupervisedUserInformation() const {
  // |browser_| can be NULL in unit_tests.
  if (browser_ && browser_->profile()->IsSupervised()) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    SupervisedUserService* service =
        SupervisedUserServiceFactory::GetForProfile(browser_->profile());
    base::string16 custodian =
        base::UTF8ToUTF16(service->GetCustodianEmailAddress());
    if (browser_->profile()->IsLegacySupervised())
      return l10n_util::GetStringFUTF16(IDS_LEGACY_SUPERVISED_USER_INFO,
                                        custodian);
    base::string16 second_custodian =
        base::UTF8ToUTF16(service->GetSecondCustodianEmailAddress());
    if (second_custodian.empty()) {
      return l10n_util::GetStringFUTF16(IDS_CHILD_INFO_ONE_CUSTODIAN,
                                        custodian);
    } else {
      return l10n_util::GetStringFUTF16(IDS_CHILD_INFO_TWO_CUSTODIANS,
                                        custodian, second_custodian);
    }
#endif
  }
  return base::string16();
}

void AvatarMenu::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;
  menu_actions_->ActiveBrowserChanged(browser);

  // Get the path of its active profile if |browser| is not NULL. Note that
  // |browser| is NULL in unit tests.
  base::FilePath path;
  if (browser)
    path = browser->profile()->GetPath();
  profile_list_->ActiveProfilePathChanged(path);
}

bool AvatarMenu::ShouldShowAddNewProfileLink() const {
  return menu_actions_->ShouldShowAddNewProfileLink();
}

bool AvatarMenu::ShouldShowEditProfileLink() const {
  return menu_actions_->ShouldShowEditProfileLink();
}

void AvatarMenu::OnProfileAdded(const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileWasRemoved(const base::FilePath& profile_path,
                                     const base::string16& profile_name) {
  Update();
}

void AvatarMenu::OnProfileNameChanged(const base::FilePath& profile_path,
                                      const base::string16& old_profile_name) {
  Update();
}

void AvatarMenu::OnProfileAuthInfoChanged(const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileAvatarChanged(const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileHighResAvatarLoaded(
    const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileSigninRequiredChanged(
    const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileIsOmittedChanged(const base::FilePath& profile_path) {
  Update();
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
void AvatarMenu::OnCustodianInfoChanged() {
  Update();
}
#endif

void AvatarMenu::Update() {
  RebuildMenu();
  if (observer_)
    observer_->OnAvatarMenuChanged(this);
}
