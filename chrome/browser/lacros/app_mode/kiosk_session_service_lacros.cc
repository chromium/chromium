// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_browser_session.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/origin.h"

namespace {

static KioskSessionServiceLacros* g_kiosk_session_service = nullptr;

bool IsWebKioskSession() {
  return chromeos::BrowserParamsProxy::Get()->SessionType() ==
         crosapi::mojom::SessionType::kWebKioskSession;
}

void AttemptUserExit() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  CHECK(service);

  if (!service->IsAvailable<crosapi::mojom::KioskSessionService>()) {
    LOG(ERROR) << "Kiosk session service is not available.";
    return;
  }

  service->GetRemote<crosapi::mojom::KioskSessionService>()->AttemptUserExit();
}

}  // namespace

// Runs callback when a new browser is opened.
class NewBrowserObserver : public BrowserListObserver {
 public:
  explicit NewBrowserObserver(
      base::RepeatingCallback<void(Browser* browser)> on_browser_added)
      : on_browser_added_(std::move(on_browser_added)) {
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }
  NewBrowserObserver(const NewBrowserObserver&) = delete;
  NewBrowserObserver& operator=(const NewBrowserObserver&) = delete;
  ~NewBrowserObserver() override = default;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    on_browser_added_.Run(browser);
  }

 private:
  base::ScopedObservation<BrowserList, NewBrowserObserver>
      browser_list_observation_{this};
  base::RepeatingCallback<void(Browser* browser)> on_browser_added_;
};

// static
KioskSessionServiceLacros* KioskSessionServiceLacros::Get() {
  CHECK(g_kiosk_session_service);
  return g_kiosk_session_service;
}

// static
void KioskSessionServiceLacros::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  chromeos::KioskBrowserSession::RegisterLocalStatePrefs(registry);
}

// static
void KioskSessionServiceLacros::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  chromeos::KioskBrowserSession::RegisterProfilePrefs(registry);
}

KioskSessionServiceLacros::KioskSessionServiceLacros()
    : attempt_user_exit_(base::BindOnce(&AttemptUserExit)) {
  // TODO(b/321669108): add CHECK(!g_kiosk_session_service)
  DUMP_WILL_BE_CHECK(!g_kiosk_session_service);
  g_kiosk_session_service = this;

  if (IsWebKioskSession()) {
    new_browser_observer_ =
        std::make_unique<NewBrowserObserver>(base::BindRepeating(
            [](base::WeakPtr<KioskSessionServiceLacros> kiosk_service,
               Browser* browser) {
              if (!kiosk_service) {
                return;
              }
              kiosk_service->KioskSessionServiceLacros::InitWebKioskSession(
                  CHECK_DEREF(browser));
              kiosk_service->new_browser_observer_.reset();
            },
            weak_factory_.GetWeakPtr()));
  }
}

KioskSessionServiceLacros::~KioskSessionServiceLacros() {
  g_kiosk_session_service = nullptr;
}

void KioskSessionServiceLacros::InitChromeKioskSession(
    Profile* profile,
    const std::string& app_id) {
  LOG_IF(FATAL, kiosk_browser_session_)
      << "Kiosk browser session is already initialized.";
  kiosk_browser_session_ = std::make_unique<chromeos::KioskBrowserSession>(
      profile, std::move(attempt_user_exit_), g_browser_process->local_state());
  kiosk_browser_session_->InitForChromeAppKiosk(app_id);
}

void KioskSessionServiceLacros::InitWebKioskSession(Browser& browser) {
  LOG_IF(FATAL, kiosk_browser_session_)
      << "Kiosk session is already initialized.";

  kiosk_browser_session_ = std::make_unique<chromeos::KioskBrowserSession>(
      browser.profile(), std::move(attempt_user_exit_),
      g_browser_process->local_state());
  kiosk_browser_session_->InitForWebKiosk(browser.app_name());
  browser.profile()
      ->GetExtensionSpecialStoragePolicy()
      ->AddOriginWithUnlimitedStorage(url::Origin::Create(install_url_));

  for (auto& observer : observers_) {
    observer.KioskWebSessionInitialized();
  }
}

void KioskSessionServiceLacros::SetInstallUrl(const GURL& install_url) {
  // `SetInstallUrl` should be called once, but if it is called second time,
  // the url should be the same.
  CHECK(install_url_.is_empty() || install_url_ == install_url)
      << "install_url_=" << install_url_ << ", install_url=" << install_url;
  install_url_ = install_url;
}

std::unique_ptr<base::AutoReset<base::OnceClosure>>
KioskSessionServiceLacros::SetAttemptUserExitCallbackForTesting(
    base::OnceClosure attempt_user_exit) {
  return std::make_unique<base::AutoReset<base::OnceClosure>>(
      base::AutoReset<base::OnceClosure>(&attempt_user_exit_,
                                         std::move(attempt_user_exit)));
}

void KioskSessionServiceLacros::AddObserver(
    KioskSessionServiceLacros::Observer* observer) {
  observers_.AddObserver(observer);
}

void KioskSessionServiceLacros::RemoveObserver(
    KioskSessionServiceLacros::Observer* observer) {
  observers_.RemoveObserver(observer);
}
