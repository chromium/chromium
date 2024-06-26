// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/avatar_menu.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu_observer.h"
#include "chrome/browser/profiles/profile_list_desktop.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;

namespace {

bool CanOpenBrowserForProfile(const AvatarMenu::Item& profile_item) {
  if (profile_item.signin_required)
    return false;

  // We can open a browser only if a profile is loaded.
  Profile* profile = g_browser_process->profile_manager()->GetProfileByPath(
      profile_item.profile_path);
  if (!profile)
    return false;

  return true;
}

}  // namespace

AvatarMenu::AvatarMenu(ProfileAttributesStorage* profile_storage,
                       AvatarMenuObserver* observer,
                       Browser* browser)
    : profile_list_(std::make_unique<ProfileListDesktop>(profile_storage)),
      profile_storage_(profile_storage->AsWeakPtr()),
      observer_(observer),
      browser_(browser) {
  DCHECK(profile_storage_);
  // Don't DCHECK(browser_) so that unit tests can reuse this ctor.

  ActiveBrowserChanged(browser_);

  // Register this as an observer of the info cache.
  profile_storage_->AddObserver(this);

  // Register this as an observer of the SupervisedUserService to be notified
  // of changes to the custodian info.
  if (browser_) {
    auto* supervised_user_service =
        SupervisedUserServiceFactory::GetForProfile(browser_->profile());
    if (supervised_user_service) {
      supervised_user_observation_.Observe(supervised_user_service);
    }
  }
}

AvatarMenu::~AvatarMenu() {
  // Note that |profile_storage_| may be destroyed before |this|.
  // https://crbug.com/1008947
  if (profile_storage_)
    profile_storage_->RemoveObserver(this);
}

AvatarMenu::Item::Item(size_t menu_index,
                       const base::FilePath& profile_path,
                       const gfx::Image& icon)
    : icon(icon),
      active(false),
      signin_required(false),
      menu_index(menu_index),
      profile_path(profile_path) {}

AvatarMenu::Item::Item(const Item& other) = default;

AvatarMenu::Item::~Item() = default;

void AvatarMenu::SwitchToProfile(size_t index, bool always_create) {
  DCHECK(profiles::IsMultipleProfilesEnabled() ||
         index == GetActiveProfileIndex());
  const Item& item = GetItemAt(index);

  if (item.signin_required) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileLocked));
    return;
  }

  profiles::SwitchToProfile(item.profile_path, always_create);
}

void AvatarMenu::AddNewProfile() {
  if (!ShouldShowAddNewProfileLink())
    return;

  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
}

void AvatarMenu::EditProfile(size_t index) {
  const Item& item = GetItemAt(index);
  if (!CanOpenBrowserForProfile(item))
    return;

  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(item.profile_path);
  DCHECK(profile);

  chrome::ShowSettingsSubPageForProfile(profile, chrome::kManageProfileSubPage);
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

size_t AvatarMenu::GetIndexOfItemWithProfilePathForTesting(
    const base::FilePath& path) const {
  std::optional<size_t> index = profile_list_->MenuIndexFromProfilePath(path);
  DCHECK(index.has_value());
  return index.value();
}

std::optional<size_t> AvatarMenu::GetActiveProfileIndex() const {
  // During singleton profile deletion, this function can be called with no
  // profiles in the model - crbug.com/102278 .
  if (profile_list_->GetNumberOfItems() == 0)
    return std::nullopt;

  Profile* active_profile = browser_
                                ? browser_->profile()
                                : ProfileManager::GetLastUsedProfileIfLoaded();

  if (!active_profile)
    return std::nullopt;

  // The profile may be missing from the menu (e.g. omitted profile, guest).
  std::optional<size_t> index =
      profile_list_->MenuIndexFromProfilePath(active_profile->GetPath());

  DCHECK(!index.has_value() ||
         index.value() < profile_list_->GetNumberOfItems());
  return index;
}

void AvatarMenu::ActiveBrowserChanged(Browser* browser) {
  browser_ = browser;

  // Get the path of its active profile if |browser| is not NULL. Note that
  // |browser| is NULL in unit tests.
  base::FilePath path;
  if (browser)
    path = browser->profile()->GetPath();
  profile_list_->ActiveProfilePathChanged(path);
}

bool AvatarMenu::ShouldShowAddNewProfileLink() const {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);
  return service->GetBoolean(prefs::kBrowserAddPersonEnabled);
}

bool AvatarMenu::ShouldShowEditProfileLink() const {
  std::optional<size_t> active_profile_index = GetActiveProfileIndex();
  if (!active_profile_index)
    return false;

  return CanOpenBrowserForProfile(GetItemAt(*active_profile_index));
}

void AvatarMenu::OnProfileAdded(const base::FilePath& profile_path) {
  Update();
}

void AvatarMenu::OnProfileWasRemoved(const base::FilePath& profile_path,
                                     const std::u16string& profile_name) {
  Update();
}

void AvatarMenu::OnProfileNameChanged(const base::FilePath& profile_path,
                                      const std::u16string& old_profile_name) {
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

void AvatarMenu::OnCustodianInfoChanged() {
  Update();
}

void AvatarMenu::Update() {
  RebuildMenu();
  if (observer_)
    observer_->OnAvatarMenuChanged(this);
}
