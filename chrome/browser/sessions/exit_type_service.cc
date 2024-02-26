// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/exit_type_service.h"

#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service_factory.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

// Value written to prefs for ExitType::kCrashed and ExitType::kForcedShutdown.
const char kPrefExitTypeCrashed[] = "Crashed";
const char kPrefExitTypeNormal[] = "Normal";
const char kPrefExitTypeForcedShutdown[] = "SessionEnded";

// Converts the `kSessionExitType` pref to the corresponding EXIT_TYPE.
ExitType SessionTypePrefValueToExitType(const std::string& value) {
  if (value == kPrefExitTypeForcedShutdown)
    return ExitType::kForcedShutdown;
  if (value == kPrefExitTypeCrashed)
    return ExitType::kCrashed;
  return ExitType::kClean;
}

// Converts an ExitType into a string that is written to prefs.
std::string ExitTypeToSessionTypePrefValue(ExitType type) {
  switch (type) {
    case ExitType::kClean:
      return kPrefExitTypeNormal;
    case ExitType::kForcedShutdown:
      return kPrefExitTypeForcedShutdown;
    case ExitType::kCrashed:
      return kPrefExitTypeCrashed;
  }
}

}  // namespace

// Responsible for notifying ExitTypeService when a browser or tab is created
// under the right circumstances.
class ExitTypeService::BrowserTabObserverImpl : public BrowserListObserver,
                                                public TabStripModelObserver {
 public:
  explicit BrowserTabObserverImpl(ExitTypeService* service)
      : service_(service) {
    BrowserList::GetInstance()->AddObserver(this);
  }

  ~BrowserTabObserverImpl() override {
    BrowserList::GetInstance()->RemoveObserver(this);
    for (Browser* browser : browsers_)
      browser->tab_strip_model()->RemoveObserver(this);
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    if (browser->profile() != service_->profile() ||
        browser->omit_from_session_restore() ||
        !SessionService::IsRelevantWindowType(
            WindowTypeForBrowserType(browser->type()))) {
      return;
    }
    if (browser->create_params().creation_source !=
        Browser::CreationSource::kStartupCreator) {
      // Ideally this would call directly to `service_`, but at the time this
      // is called it is too early to do that. So, this waits for the first tab
      // to be added.
      // be added.
      browsers_.insert(browser);
      browser->tab_strip_model()->AddObserver(this);
    }
  }

  void OnBrowserRemoved(Browser* browser) override {
    auto iter = browsers_.find(browser);
    if (iter == browsers_.end())
      return;
    browser->tab_strip_model()->RemoveObserver(this);
    browsers_.erase(iter);
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kInserted)
      service_->OnTabAddedToNonStartupBrowser();
  }

 private:
  raw_ptr<ExitTypeService> service_;

  // Browsers whose TabStripModel this is observing.
  base::flat_set<raw_ptr<Browser, CtnExperimental>> browsers_;
};

ExitTypeService::CrashedLock::~CrashedLock() {
  service_->CrashedLockDestroyed();
}

ExitTypeService::CrashedLock::CrashedLock(ExitTypeService* service)
    : service_(service) {}

ExitTypeService::~ExitTypeService() {
  // If the user did not takesome action that would constitute a new session
  // (such as closing the crash bubble, or creating a new browser), do not
  // reset the crash status. This way the user will have another chance to
  // restore when chrome starts again.
  if (!waiting_for_user_to_ack_crash_)
    SetCurrentSessionExitType(ExitType::kClean);
}

// static
ExitTypeService* ExitTypeService::GetInstanceForProfile(Profile* profile) {
  return ExitTypeServiceFactory::GetForProfile(profile);
}

// static
ExitType ExitTypeService::GetLastSessionExitType(Profile* profile) {
  ExitTypeService* exit_type_service =
      GetInstanceForProfile(profile->GetOriginalProfile());
  // `exit_type_service` may be null for certain profile types (such as signin
  // profile on chromeos).
  return exit_type_service ? exit_type_service->last_session_exit_type()
                           : ExitType::kClean;
}

void ExitTypeService::SetCurrentSessionExitType(ExitType exit_type) {
  if (waiting_for_user_to_ack_crash_) {
    if (!exit_type_to_apply_on_ack_.has_value())
      exit_type_to_apply_on_ack_ = exit_type;
    return;
  }

  // This may be invoked multiple times during shutdown. Only persist the value
  // first passed in.
  if (exit_type == ExitType::kCrashed ||
      current_session_exit_type_ == ExitType::kCrashed) {
    current_session_exit_type_ = exit_type;
    profile_->GetPrefs()->SetString(prefs::kSessionExitType,
                                    ExitTypeToSessionTypePrefValue(exit_type));
  }
}

std::unique_ptr<ExitTypeService::CrashedLock>
ExitTypeService::CreateCrashedLock() {
  if (!waiting_for_user_to_ack_crash_)
    return nullptr;

  ++crashed_lock_count_;
  // Uses WrapUnique() as constructor is private.
  return base::WrapUnique(new CrashedLock(this));
}

void ExitTypeService::AddCrashAckCallback(base::OnceClosure callback) {
  // `waiting_for_user_to_ack_crash_` never goes from false to true. So any
  // callback added when false can be ignored.
  if (!waiting_for_user_to_ack_crash_)
    return;
  crash_ack_callbacks_.push_back(std::move(callback));
}

ExitTypeService::ExitTypeService(Profile* profile)
    : profile_(profile),
      last_session_exit_type_(SessionTypePrefValueToExitType(
          profile_->GetPrefs()->GetString(prefs::kSessionExitType))),
      current_session_exit_type_(ExitType::kCrashed),
      waiting_for_user_to_ack_crash_(last_session_exit_type_ ==
                                     ExitType::kCrashed) {
  // Mark the session as open.
  profile_->GetPrefs()->SetString(prefs::kSessionExitType,
                                  kPrefExitTypeCrashed);
  if (waiting_for_user_to_ack_crash_)
    browser_tab_observer_ = std::make_unique<BrowserTabObserverImpl>(this);
}

void ExitTypeService::CheckUserAckedCrash() {
  // Once shutdown has started the state this checks may be torn down. During
  // tear down we shouldn't run the callbacks, as the user did not really ack
  // the crash.
  if (browser_shutdown::HasShutdownStarted())
    return;

  if (!DidUserAckCrash())
    return;

  if (!IsWaitingForRestore() && SessionRestore::IsRestoring(profile_) &&
      !did_restore_complete_) {
    // Wait for restore to finish before enabling. This way if there is a crash
    // before the actual restore, the last session isn't lost.
    restore_subscription_ =
        SessionRestore::RegisterOnSessionRestoredCallback(base::BindRepeating(
            &ExitTypeService::OnSessionRestoreDone, base::Unretained(this)));
    return;
  }

  waiting_for_user_to_ack_crash_ = false;

  if (exit_type_to_apply_on_ack_.has_value())
    SetCurrentSessionExitType(*exit_type_to_apply_on_ack_);

  std::vector<base::OnceClosure> callbacks;
  std::swap(callbacks, crash_ack_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void ExitTypeService::CrashedLockDestroyed() {
  --crashed_lock_count_;
  DCHECK_GE(crashed_lock_count_, 0);
  if (crashed_lock_count_ == 0)
    CheckUserAckedCrash();
}

void ExitTypeService::OnTabAddedToNonStartupBrowser() {
  browser_tab_observer_.reset();
  CheckUserAckedCrash();
}

bool ExitTypeService::DidUserAckCrash() const {
  // See class description for details on this.
  return waiting_for_user_to_ack_crash_ && !IsWaitingForRestore() &&
         (!IsWaitingForBrowser() || (crashed_lock_count_ == 0));
}

void ExitTypeService::OnSessionRestoreDone(Profile* profile,
                                           int tabs_restored) {
  if (profile != profile_)
    return;
  did_restore_complete_ = true;
  restore_subscription_ = {};
  CheckUserAckedCrash();
}
