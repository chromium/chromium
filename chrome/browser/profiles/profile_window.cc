// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

using base::UserMetricsAction;
using content::BrowserThread;

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
void UnblockExtensions(Profile* profile) {
  extensions::ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extension_service->UnblockAllExtensions();
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

// Helper function to run a callback on a profile once it's initialized.
void ProfileLoadedCallback(base::OnceCallback<void(Profile*)> callback,
                           Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!profile) {
    return;
  }
  if (callback) {
    std::move(callback).Run(profile);
  }
}

}  // namespace

namespace profiles {

void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool always_create) {
  DCHECK(profile);
  TRACE_EVENT1("browser", "FindOrCreateNewWindowForProfile", "profile_path",
               profile->GetPath());

  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  base::RecordAction(UserMetricsAction("NewWindow"));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  StartupBrowserCreator browser_creator;
  // This is not a browser launch from the user; don't record the launch mode.
  browser_creator.LaunchBrowser(command_line, profile, base::FilePath(),
                                process_startup, is_first_run,
                                /*restore_tabbed_browser=*/true);
}

void OpenBrowserWindowForProfile(base::OnceCallback<void(Browser*)> callback,
                                 bool always_create,
                                 bool is_new_profile,
                                 bool unblock_extensions,
                                 Profile* profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("browser", "OpenBrowserWindowForProfile", "profile_path",
               profile->GetPath().AsUTF8Unsafe());
  // `error_closure_runner` runs the callback  with nullptr to signal an error
  // if the function reaches a return statement without consuming callback. If
  // the callback is consumed by std::move(), then `callback` will be empty
  // after that and the scoped cleanup does nothing.
  absl::Cleanup error_closure_runner([&callback] {
    if (callback) {
      std::move(callback).Run(nullptr);
    }
  });
  chrome::startup::IsProcessStartup process_startup =
      chrome::startup::IsProcessStartup::kNo;
  chrome::startup::IsFirstRun is_first_run = chrome::startup::IsFirstRun::kNo;

  // If this is a brand new profile, then start a first run window.
  if (is_new_profile) {
    process_startup = chrome::startup::IsProcessStartup::kYes;
    is_first_run = chrome::startup::IsFirstRun::kYes;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (!profile->IsGuestSession()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());
    if (entry && entry->IsSigninRequired()) {
      ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
          ProfilePicker::EntryPoint::kProfileLocked));
      return;
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (unblock_extensions) {
    UnblockExtensions(profile);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // If |always_create| is false, and we have a |callback| to run, check
  // whether a browser already exists so that we can run the callback. We don't
  // want to rely on the observer listening to OnBrowserSetLastActive in this
  // case, as you could manually activate an incorrect browser and trigger
  // a false positive.
  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false);
    if (browser) {
      browser->window()->Activate();
      if (callback) {
        std::move(callback).Run(browser);
      }
      return;
    }
  }

  // If there is a callback, create an observer to make sure it is only
  // run when the browser has been completely created. This observer will
  // delete itself once that happens. This should not leak, because we are
  // passing |always_create| = true to FindOrCreateNewWindow below, which ends
  // up calling LaunchBrowser and opens a new window. If for whatever reason
  // that fails, either something has crashed, or the observer will be cleaned
  // up when a different browser for this profile is opened.
  if (callback) {
    new BrowserAddedForProfileObserver(profile, std::move(callback));
  }

  // We already dealt with the case when |always_create| was false and a browser
  // existed, which means that here a browser definitely needs to be created.
  // Passing true for |always_create| means we won't duplicate the code that
  // tries to find a browser.
  profiles::FindOrCreateNewWindowForProfile(profile, process_startup,
                                            is_first_run, true);
}

#if !BUILDFLAG(IS_ANDROID)

void LoadProfileAsync(const base::FilePath& path,
                      base::OnceCallback<void(Profile*)> callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path, base::BindOnce(&ProfileLoadedCallback, std::move(callback)));
}

void SwitchToProfile(const base::FilePath& path,
                     bool always_create,
                     base::OnceCallback<void(Browser*)> callback) {
  base::OnceCallback<void(Profile*)> open_browser_callback =
      base::BindOnce(&profiles::OpenBrowserWindowForProfile,
                     std::move(callback), always_create,
                     /*is_new_profile=*/false,
                     /*unblock_extensions=*/false);
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::BindOnce(&ProfileLoadedCallback, std::move(open_browser_callback)));
}

void SwitchToGuestProfile(base::OnceCallback<void(Browser*)> callback) {
  SwitchToProfile(ProfileManager::GetGuestProfilePath(),
                  /*always_create=*/false, std::move(callback));
}
#endif

bool HasProfileSwitchTargets(Profile* profile) {
  size_t min_profiles = profile->IsGuestSession() ? 1 : 2;
  size_t number_of_profiles =
      g_browser_process->profile_manager()->GetNumberOfProfiles();
  return number_of_profiles >= min_profiles;
}

void CloseProfileWindows(Profile* profile) {
  DCHECK(profile);
  BrowserList::CloseAllBrowsersWithProfile(profile,
                                           BrowserList::CloseCallback(),
                                           BrowserList::CloseCallback(), false);
}

BrowserAddedForProfileObserver::BrowserAddedForProfileObserver(
    Profile* profile,
    base::OnceCallback<void(Browser*)> callback)
    : profile_(profile->GetWeakPtr()), callback_(std::move(callback)) {
  DCHECK(callback_);
  browser_list_observation_.Observe(BrowserList::GetInstance());
}

BrowserAddedForProfileObserver::~BrowserAddedForProfileObserver() {}

void BrowserAddedForProfileObserver::OnBrowserAdded(Browser* browser) {
  if (browser_) {
    // Do not run the callback twice.
    return;
  }

  if (browser->profile() != profile_.get()) {
    // The profile has been deleted, or this is a different profile.
    return;
  }

  browser_ = browser;
  // By the time the browser is added a tab (or multiple) are about to be added.
  // Post the callback to the message loop so it gets executed after the tabs
  // are created.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserAddedForProfileObserver::NotifyBrowserCreatedAnDie,
                     base::Unretained(this)));
}

void BrowserAddedForProfileObserver::OnBrowserRemoved(Browser* browser) {
  // The browser was closed before the callback could run.
  if (browser == browser_) {
    browser_list_observation_.Reset();
    browser_ = nullptr;
  }
}

void BrowserAddedForProfileObserver::NotifyBrowserCreatedAnDie() {
  // If the browser exists, the profile has to exist too.
  CHECK(profile_ || !browser_);
  std::move(callback_).Run(browser_);
  delete this;
}

}  // namespace profiles
