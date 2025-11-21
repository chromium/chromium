// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_util.h"

#include "base/task/current_thread.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/glic/widget/local_hotkey_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

GlicInstanceCoordinatorImpl& GetInstanceCoordinator(GlicKeyedService& service) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicMultiInstance));
  return static_cast<GlicInstanceCoordinatorImpl&>(service.window_controller());
}

}  // namespace

BrowserActivator::BrowserActivator() {
  BrowserList::AddObserver(this);
}

BrowserActivator::~BrowserActivator() {
  BrowserList::RemoveObserver(this);
}

void BrowserActivator::SetMode(Mode mode) {
  mode_ = mode;
}

void BrowserActivator::OnBrowserAdded(Browser* browser) {
  switch (mode_) {
    case Mode::kSingleBrowser:
      CHECK(!active_browser_) << "BrowserActivator::kSingleBrowser found "
                                 "second active browser.";
      break;
    case Mode::kFirst:
      if (active_browser_) {
        return;
      }
      break;
    case Mode::kManual:
      return;
  }

  SetActivePrivate(browser);
}

void BrowserActivator::OnBrowserRemoved(Browser* browser) {
  if (active_browser_.get() == browser || active_browser_.WasInvalidated()) {
    active_lock_.reset();
    if (mode_ == Mode::kFirst) {
      if (auto* const browser_window_interface =
              GetLastActiveBrowserWindowInterfaceWithAnyProfile()) {
        SetActivePrivate(browser_window_interface);
      }
    }
  }
}

void BrowserActivator::SetActive(Browser* browser) {
  mode_ = Mode::kManual;
  if (!browser) {
    active_lock_.reset();
    active_browser_ = nullptr;
  } else {
    SetActivePrivate(browser);
  }
}

void BrowserActivator::SetActivePrivate(
    BrowserWindowInterface* browser_window_interface) {
  CHECK(browser_window_interface);
  if (auto* const browser_view =
          BrowserView::GetBrowserViewForBrowser(browser_window_interface)) {
    active_lock_ = browser_view->GetWidget()->LockPaintAsActive();
    active_browser_ = browser_window_interface->GetWeakPtr();
  }
}

GlicInstanceTracker::GlicInstanceTracker(Profile* profile)
    : profile_(profile) {}
GlicInstanceTracker::~GlicInstanceTracker() = default;
void GlicInstanceTracker::SetProfile(Profile* profile) {
  profile_ = profile;
}

Host* GlicInstanceTracker::GetHost() {
  auto* instance = GetGlicInstance();
  if (!instance) {
    return nullptr;
  }
  return &instance->host();
}

GlicInstance* GlicInstanceTracker::GetGlicInstance() {
  if (!profile_) {
    return nullptr;
  }
  auto* service = GlicKeyedService::Get(profile_);
  if (!service) {
    return nullptr;
  }
  if (tracked_instance_id_) {
    for (GlicInstance* instance : service->window_controller().GetInstances()) {
      if (instance->id() == *tracked_instance_id_) {
        return instance;
      }
    }
    return nullptr;
  }
  if (track_only_glic_instance_) {
    auto instances = service->window_controller().GetInstances();
    // Ignore the warming instance.
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      auto iter = std::find(
          instances.begin(), instances.end(),
          GetInstanceCoordinator(*service).GetWarmedInstanceForTesting());
      if (iter != instances.end()) {
        instances.erase(iter);
      }
    }
    CHECK_LT(instances.size(), 2u);
    return instances.empty() ? nullptr : instances[0];
  }

  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    if (track_floating_glic_instance_) {
      return GetInstanceCoordinator(*service).GetInstanceWithFloaty();
    }
    if (glic_instance_tab_handle_) {
      if (glic_instance_tab_handle_->Get()) {
        return service->GetInstanceForTab(glic_instance_tab_handle_->Get());
      }
      return nullptr;
    }
    if (glic_instance_tab_index_ != std::nullopt) {
      return service->GetInstanceForTab(
          GetBrowser()->GetTabStripModel()->GetTabAtIndex(
              *glic_instance_tab_index_));
    }
    return service->GetInstanceForTab(
        GetBrowser()->GetTabStripModel()->GetTabAtIndex(0));
  }
  return service->GetInstanceForActiveTab(GetBrowser());
}

BrowserWindowInterface* GlicInstanceTracker::GetBrowser() {
  BrowserWindowInterface* found = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &found](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile_) {
          found = browser;
        }
        return !found;
      });
  return found;
}

std::string GlicInstanceTracker::DescribeGlicTracking() {
  if (tracked_instance_id_) {
    return base::StrCat({"Tracking glic instance with id ",
                         tracked_instance_id_->AsLowercaseString()});
  } else if (glic_instance_tab_index_) {
    return base::StrCat({"Tracking glic instance at tab index ",
                         base::NumberToString(*glic_instance_tab_index_)});

  } else if (glic_instance_tab_handle_) {
    if (!glic_instance_tab_handle_->Get()) {
      return "Tracking glic instance with INVALID tab handle";
    }
    return "Tracking glic instance with tab handle";
  } else if (track_floating_glic_instance_) {
    return "Tracking floating glic instance";
  }
  NOTREACHED();
}

void GlicInstanceTracker::Clear() {
  tracked_instance_id_ = std::nullopt;
  glic_instance_tab_index_ = std::nullopt;
  glic_instance_tab_handle_ = std::nullopt;
  track_floating_glic_instance_ = false;
}

[[nodiscard]] bool GlicInstanceTracker::WaitForPanelState(
    mojom::PanelStateKind state) {
  // TODO(harringtond): Use observers instead of polling.
  return base::test::RunUntil([&]() {
    auto* instance = GetGlicInstance();
    if (!instance) {
      return false;
    }
    return instance->GetPanelState().kind == state;
  });
}

[[nodiscard]] bool GlicInstanceTracker::WaitForShow() {
  // TODO(harringtond): Use observers instead of polling.
  return base::test::RunUntil([&]() {
    auto* instance = GetGlicInstance();
    if (!instance) {
      return false;
    }
    return instance->IsShowing();
  });
}

void ForceSigninAndModelExecutionCapability(Profile* profile) {
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  SigninWithPrimaryAccount(profile);
  SetModelExecutionCapability(profile, true);
}

void SigninWithPrimaryAccount(Profile* profile) {
  // Sign-in and enable account capability.
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  auto account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "glic-test@example.com", signin::ConsentLevel::kSignin);
  ASSERT_FALSE(account_info.IsEmpty());

  account_info.full_name = "Glic Testing";
  account_info.given_name = "Glic";
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
}

void SetModelExecutionCapability(Profile* profile, bool enabled) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account =
      identity_manager->FindExtendedAccountInfoByAccountId(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(primary_account.IsEmpty());

  AccountCapabilitiesTestMutator mutator(&primary_account.capabilities);
  mutator.set_can_use_model_execution_features(enabled);
  signin::UpdateAccountInfoForAccount(identity_manager, primary_account);
}

void SetFRECompletion(Profile* profile, prefs::FreStatus fre_status) {
  profile->GetPrefs()->SetInteger(prefs::kGlicCompletedFre,
                                  static_cast<int>(fre_status));
}

void InvalidateAccount(Profile* profile) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  ASSERT_TRUE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          identity_manager->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin)));
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    ASSERT_FALSE(
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync));
  }
  ASSERT_TRUE(
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

void ReauthAccount(Profile* profile) {
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      GoogleServiceAuthError::AuthErrorNone());
}

}  // namespace glic
