// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/turn_sync_on_helper.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/core_account_id.h"

namespace {

// Whether the user has seen the sync first-run dialog. They might or might not
// have consented to sync. Other platforms don't need this because they can
// infer the no-consent state from IdentityManager::GetPrimaryAccount()
// returning an empty account, but on Chrome OS the primary user isn't allowed
// to sign out.
const char kSyncFirstRunCompleted[] = "sync.first_run_completed";

// Ensures a tabbed browser is visible and returns it.
Browser* EnsureBrowser(Browser* browser, Profile* profile) {
  if (!browser) {
    // The user has closed the browser that we used previously. Grab the most
    // recently active browser or else create a new one.
    browser = chrome::FindLastActiveWithProfile(profile);
    if (!browser) {
      // The browser deletes itself.
      browser = Browser::Create(
          Browser::CreateParams(profile, /*user_gesture=*/true));
      chrome::AddTabAt(browser, GURL(), /*index=*/-1, /*foreground=*/true);
    }
  }
  browser->window()->Show();
  return browser;
}

// Production delegate with real UI.
class DelegateImpl : public TurnSyncOnHelper::Delegate {
 public:
  DelegateImpl() = default;
  ~DelegateImpl() override = default;

  void ShowSyncConfirmation(Profile* profile, Browser* browser) override {
    browser = EnsureBrowser(browser, profile);
    browser->signin_view_controller()->ShowModalSyncConfirmationDialog();
  }

  void ShowSyncSettings(Profile* profile, Browser* browser) override {
    browser = EnsureBrowser(browser, profile);
    chrome::ShowSettingsSubPage(browser, chrome::kSyncSetupSubPage);
  }
};

}  // namespace

TurnSyncOnHelper::TurnSyncOnHelper(Profile* profile)
    : TurnSyncOnHelper(profile, std::make_unique<DelegateImpl>()) {}

TurnSyncOnHelper::TurnSyncOnHelper(Profile* profile,
                                   std::unique_ptr<Delegate> delegate)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      delegate_(std::move(delegate)) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(chromeos::features::ShouldUseBrowserSyncConsent());
  Init();
}

TurnSyncOnHelper::~TurnSyncOnHelper() {
  BrowserList::RemoveObserver(this);
}

// static
void TurnSyncOnHelper::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSyncFirstRunCompleted, false);
}

void TurnSyncOnHelper::Init() {
  // Skipping first-run.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kForceFirstRun) &&
      command_line->HasSwitch(switches::kNoFirstRun)) {
    return;
  }

  // Already consented to sync.
  if (identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return;

  // Previously completed setup (and chose not to sync).
  if (profile_->GetPrefs()->GetBoolean(kSyncFirstRunCompleted))
    return;

  // Watch for new windows.
  BrowserList::AddObserver(this);
}

void TurnSyncOnHelper::OnBrowserSetLastActive(Browser* browser) {
  // Tabbed browser window (not an app).
  if (!browser->is_type_normal())
    return;

  // For multi-login sessions, only trigger for windows for this profile.
  if (browser->profile() != profile_)
    return;

  // Not guest or incognito window.
  if (browser->profile()->IsOffTheRecord())
    return;

  browser_ = browser;
  BrowserList::RemoveObserver(this);
  StartFlow();
}

void TurnSyncOnHelper::StartFlow() {
  syncer::SyncService* sync_service = GetSyncService();
  // Abort if sync not allowed. Check here instead of in Init() just in case
  // enterprise policy was updated before the first window opened.
  if (!sync_service)
    return;

  // Record that the user has started the flow ("requested setup").
  sync_service->GetUserSettings()->SetSyncRequested(true);

  // Defer if sync engine is still starting up.
  if (SyncStartupTracker::GetSyncServiceState(sync_service) ==
      SyncStartupTracker::SYNC_STARTUP_PENDING) {
    sync_startup_tracker_ =
        std::make_unique<SyncStartupTracker>(sync_service, this);
    return;
  }

  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::SyncStartupCompleted() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::SyncStartupFailed() {
  DCHECK(sync_startup_tracker_);
  sync_startup_tracker_.reset();
  ShowSyncConfirmationUI();
}

void TurnSyncOnHelper::ShowSyncConfirmationUI() {
  // Register as an observer so OnSyncConfirmationUIClosed() will be called.
  scoped_login_ui_service_observer_.Add(
      LoginUIServiceFactory::GetForProfile(profile_));
  delegate_->ShowSyncConfirmation(profile_, browser_);
}

void TurnSyncOnHelper::OnSyncConfirmationUIClosed(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  FinishSyncSetup(result);
  // This method can be called if the browser is closed from the shelf menu
  // with the consent dialog open. Make sure we don't keep a deleted pointer.
  browser_ = nullptr;
}

void TurnSyncOnHelper::FinishSyncSetup(
    LoginUIService::SyncConfirmationUIClosedResult result) {
  unified_consent::UnifiedConsentService* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(profile_);

  switch (result) {
    case LoginUIService::CONFIGURE_SYNC_FIRST:
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      delegate_->ShowSyncSettings(profile_, browser_);
      break;
    case LoginUIService::SYNC_WITH_DEFAULT_SETTINGS: {
      syncer::SyncService* sync_service = GetSyncService();
      if (sync_service) {
        sync_service->GetUserSettings()->SetFirstSetupComplete(
            syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);
      }
      if (consent_service)
        consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(true);
      CoreAccountId account_id =
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
      DCHECK(!account_id.empty());
      // TODO(https://crbug.com/1046746): Switch to consent-aware API
      // PrimaryAccountMutator::GrantSyncConsent() when that exists.
      identity_manager_->GetPrimaryAccountMutator()->SetPrimaryAccount(
          account_id);
      break;
    }
    case LoginUIService::ABORT_SYNC:
    case LoginUIService::UI_CLOSED:
      // Chrome OS users stay signed in even if sync setup is cancelled.
      break;
  }
  profile_->GetPrefs()->SetBoolean(kSyncFirstRunCompleted, true);
}

syncer::SyncService* TurnSyncOnHelper::GetSyncService() {
  return ProfileSyncServiceFactory::IsSyncAllowed(profile_)
             ? ProfileSyncServiceFactory::GetForProfile(profile_)
             : nullptr;
}
