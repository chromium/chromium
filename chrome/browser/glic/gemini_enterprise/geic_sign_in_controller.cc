// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_sign_in_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/gemini_enterprise/geic_managed_tab.h"
#include "chrome/browser/glic/gemini_enterprise/geic_url_allowlist.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

namespace {
const int kGeicSignInControllerUserDataKey = 0;
}  // namespace

// Observes the sign-in tab opened by `StartSignIn()` and notifies
// `GeicSignInController` once the auth redirect chain commits back on the
// Gemini Enterprise guest origin with a `csesidx` session index, or when the
// sign-in tab is closed before completing sign-in.
class GeicSignInController::SignInObserver
    : public content::WebContentsObserver {
 public:
  SignInObserver(content::WebContents* web_contents,
                 url::Origin expected_guest_origin,
                 base::OnceCallback<void(std::string)> on_completed,
                 base::OnceClosure on_destroyed)
      : content::WebContentsObserver(web_contents),
        expected_guest_origin_(std::move(expected_guest_origin)),
        on_completed_(std::move(on_completed)),
        on_destroyed_(std::move(on_destroyed)) {}
  ~SignInObserver() override = default;

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!on_completed_ || !navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        navigation_handle->IsErrorPage()) {
      return;
    }
    const GURL& url = navigation_handle->GetURL();
    if (!url.SchemeIsHTTPOrHTTPS() ||
        !url::Origin::Create(url).IsSameOriginWith(expected_guest_origin_)) {
      return;
    }
    std::string csesidx;
    if (!net::GetValueForKeyInQuery(url, geic::kSessionIndexQueryParam,
                                    &csesidx) ||
        csesidx.empty()) {
      return;
    }
    Observe(nullptr);
    on_destroyed_.Reset();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(on_completed_), std::move(csesidx)));
  }

  void WebContentsDestroyed() override {
    Observe(nullptr);
    on_completed_.Reset();
    if (on_destroyed_) {
      std::move(on_destroyed_).Run();
    }
  }

 private:
  const url::Origin expected_guest_origin_;
  base::OnceCallback<void(std::string)> on_completed_;
  base::OnceClosure on_destroyed_;
};

GeicSignInController::GeicSignInController(Profile* profile)
    : profile_(profile) {}

GeicSignInController::~GeicSignInController() = default;

// static
GeicSignInController* GeicSignInController::GetForProfile(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  auto* controller = static_cast<GeicSignInController*>(
      profile->GetUserData(&kGeicSignInControllerUserDataKey));
  if (!controller) {
    auto new_controller = std::make_unique<GeicSignInController>(profile);
    controller = new_controller.get();
    profile->SetUserData(&kGeicSignInControllerUserDataKey,
                         std::move(new_controller));
  }
  return controller;
}

// static
GeicSignInController* GeicSignInController::GetIfExists(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  return static_cast<GeicSignInController*>(
      profile->GetUserData(&kGeicSignInControllerUserDataKey));
}

void GeicSignInController::OnSignInRequired(const GURL& sign_in_url) {
  csesidx_.clear();
  last_intercepted_signin_url_ = sign_in_url;
}

base::CallbackListSubscription
GeicSignInController::RegisterSignInCompletedCallback(
    base::RepeatingClosure callback) {
  return sign_in_completed_callbacks_.Add(std::move(callback));
}

GURL GeicSignInController::ResolveGuestUrl(GURL guest_url) const {
  if (!csesidx_.empty()) {
    guest_url = net::AppendOrReplaceQueryParameter(
        guest_url, geic::kSessionIndexQueryParam, csesidx_);
  }
  return guest_url;
}

void GeicSignInController::StartSignIn(const GURL& guest_url,
                                       base::OnceClosure on_completed) {
  const GURL signin_url =
      geic::BuildWorkforceSignInTabUrl(guest_url, last_intercepted_signin_url_);
  if (!signin_url.is_valid()) {
    return;
  }
  const url::Origin guest_origin = url::Origin::Create(guest_url);
  if (!signin_tab_) {
    signin_tab_ = std::make_unique<GeicManagedTab>(
        profile_, mojom::AuthTabPurpose::kSignIn,
        GaiaUrls::GetInstance()->gaia_origin(), guest_origin);
  }
  if (!signin_tab_->Open(signin_url, GeicManagedTab::ReuseMode::kActivate)) {
    return;
  }
  tabs::TabInterface* tab = signin_tab_->GetTab();
  if (!tab || !tab->GetContents()) {
    return;
  }
  StartObserving(tab->GetContents(), guest_origin, std::move(on_completed));
}

void GeicSignInController::ObserveSignInContentsForTesting(
    content::WebContents* web_contents,
    const url::Origin& guest_origin,
    base::OnceClosure on_completed) {
  StartObserving(web_contents, guest_origin, std::move(on_completed));
}

void GeicSignInController::StartObserving(content::WebContents* web_contents,
                                          const url::Origin& guest_origin,
                                          base::OnceClosure on_completed) {
  if (on_completed) {
    on_signin_callbacks_.push_back(std::move(on_completed));
  }
  if (signin_observer_ && signin_observer_->web_contents() == web_contents) {
    return;
  }
  signin_observer_ = std::make_unique<SignInObserver>(
      web_contents, guest_origin,
      base::BindOnce(&GeicSignInController::OnSignInTabCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&GeicSignInController::OnSignInTabDestroyed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GeicSignInController::OnSignInTabCompleted(std::string csesidx) {
  signin_observer_.reset();
  csesidx_ = std::move(csesidx);
  last_intercepted_signin_url_ = GURL();
  auto signin_callbacks = std::exchange(on_signin_callbacks_, {});
  if (signin_tab_) {
    signin_tab_->Close();
  }
  sign_in_completed_callbacks_.Notify();
  for (auto& callback : signin_callbacks) {
    std::move(callback).Run();
  }
}

void GeicSignInController::OnSignInTabDestroyed() {
  signin_observer_.reset();
}

}  // namespace glic
