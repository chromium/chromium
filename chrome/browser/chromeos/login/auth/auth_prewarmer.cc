// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/auth_prewarmer.h"

#include <stddef.h>

#include "base/task/post_task.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace chromeos {

AuthPrewarmer::AuthPrewarmer() : doing_prewarm_(false) {}

AuthPrewarmer::~AuthPrewarmer() {
  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
}

void AuthPrewarmer::PrewarmAuthentication(
    base::OnceClosure completion_callback) {
  if (doing_prewarm_) {
    LOG(ERROR) << "PrewarmAuthentication called twice.";
    return;
  }
  doing_prewarm_ = true;
  completion_callback_ = std::move(completion_callback);

  if (IsNetworkConnected()) {
    DoPrewarm();
  } else {
    // DefaultNetworkChanged will get called when a network becomes connected.
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
  }
}

void AuthPrewarmer::DefaultNetworkChanged(const NetworkState* network) {
  if (!network)
    return;  // Still no default (connected) network.

  NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                 FROM_HERE);
  DoPrewarm();
}

void AuthPrewarmer::DoPrewarm() {
  const int kConnectionsNeeded = 1;
  const bool kAllowCredentials = true;
  const GURL& url = GaiaUrls::GetInstance()->service_login_url();
  network::mojom::NetworkContext* network_context =
      login::GetSigninNetworkContext();
  if (network_context) {
    // Do nothing if NetworkContext isn't available.
    network_context->PreconnectSockets(
        kConnectionsNeeded, url, kAllowCredentials, net::NetworkIsolationKey());
  }
  if (!completion_callback_.is_null()) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   std::move(completion_callback_));
  }
}

bool AuthPrewarmer::IsNetworkConnected() const {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return (nsh->ConnectedNetworkByType(NetworkTypePattern::Default()) != NULL);
}

}  // namespace chromeos
