// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/android_sms/android_sms_pairing_state_tracker_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/android_sms/android_sms_urls.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace {
const char kMessagesPairStateCookieName[] = "pair_state_cookie";
const char kPairedCookieValue[] = "true";
}  // namespace

namespace ash {
namespace android_sms {

AndroidSmsPairingStateTrackerImpl::AndroidSmsPairingStateTrackerImpl(
    Profile* profile,
    AndroidSmsAppManager* android_sms_app_manager)
    : profile_(profile), android_sms_app_manager_(android_sms_app_manager) {
  android_sms_app_manager_->AddObserver(this);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AndroidSmsPairingStateTrackerImpl::AddCookieChangeListener,
          weak_ptr_factory_.GetWeakPtr()));
}

AndroidSmsPairingStateTrackerImpl::~AndroidSmsPairingStateTrackerImpl() {
  android_sms_app_manager_->RemoveObserver(this);
}

bool AndroidSmsPairingStateTrackerImpl::IsAndroidSmsPairingComplete() {
  return was_paired_on_last_update_;
}

void AndroidSmsPairingStateTrackerImpl::AttemptFetchMessagesPairingState() {
  GetCookieManager()->GetCookieList(
      GetPairingUrl(), net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&AndroidSmsPairingStateTrackerImpl::OnCookiesRetrieved,
                     base::Unretained(this)));
}

void AndroidSmsPairingStateTrackerImpl::OnCookiesRetrieved(
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  bool was_previously_paired = was_paired_on_last_update_;
  for (const auto& cookie_with_access_result : cookies) {
    const net::CanonicalCookie& cookie = cookie_with_access_result.cookie;
    if (cookie.Name() == kMessagesPairStateCookieName) {
      PA_LOG(VERBOSE) << "Cookie says Messages paired: " << cookie.Value();
      was_paired_on_last_update_ = cookie.Value() == kPairedCookieValue;
      if (was_previously_paired != was_paired_on_last_update_)
        NotifyPairingStateChanged();
      return;
    }
  }

  PA_LOG(INFO) << "No Pairing cookie found";
  was_paired_on_last_update_ = false;
  if (was_previously_paired != was_paired_on_last_update_)
    NotifyPairingStateChanged();
}

void AndroidSmsPairingStateTrackerImpl::OnCookieChange(
    const net::CookieChangeInfo& change) {
  DCHECK_EQ(kMessagesPairStateCookieName, change.cookie.Name());
  DCHECK(change.cookie.IsDomainMatch(GetPairingUrl().host()));

  // NOTE: cookie.Value() cannot be trusted in this callback. The cookie may
  // have expired or been removed and the Value() does not get updated. It's
  // cleanest to just re-fetch it.
  AttemptFetchMessagesPairingState();
}

void AndroidSmsPairingStateTrackerImpl::OnInstalledAppUrlChanged() {
  // If the app URL changed, stop any ongoing cookie monitoring and attempt to
  // add a new change listener.
  PA_LOG(INFO) << "Installed app url changed to " << GetPairingUrl()
               << ". Updating cookie listeners.";
  cookie_listener_receiver_.reset();
  AddCookieChangeListener();
}

GURL AndroidSmsPairingStateTrackerImpl::GetPairingUrl() {
  // If the app registry is not ready, we can't see check what is currently
  // installed.
  if (android_sms_app_manager_->IsAppRegistryReady()) {
    std::optional<GURL> app_url = android_sms_app_manager_->GetCurrentAppUrl();
    if (app_url)
      return *app_url;
  }

  // If no app is installed or the app registry is not ready, default to the
  // expected messages URL.  This will only be incorrect if a migration must
  // happen.
  return GetAndroidMessagesURL();
}

network::mojom::CookieManager*
AndroidSmsPairingStateTrackerImpl::GetCookieManager() {
  return profile_->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess();
}

void AndroidSmsPairingStateTrackerImpl::AddCookieChangeListener() {
  // Trigger the first fetch of the sms cookie and start listening for changes.
  AttemptFetchMessagesPairingState();
  GetCookieManager()->AddCookieChangeListener(
      GetPairingUrl(), kMessagesPairStateCookieName,
      cookie_listener_receiver_.BindNewPipeAndPassRemote());
}

}  // namespace android_sms
}  // namespace ash
