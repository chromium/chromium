// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"

class ApplicationLocaleStorage;
class PrefService;

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserPolicyConnectorAsh;
}  // namespace policy

namespace ash {

// Shows the Chrome OS out-of-box / login UI. The LoginDisplayHost may be
// created asynchronously, after a locale switch.
// `local_state`, `metrics_service`, `application_locale_storage` and
// `browser_policy_connector_ash` must be non-null and must outlive the
// LoginDisplayHost that this function creates.
// `shared_url_loader_factory` must be non-null.
void ShowLoginWizard(
    PrefService* local_state,
    ::metrics::MetricsService* metrics_service,
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    OobeScreenId first_screen);

// Shuts down LoginDisplayHostWebUI host and create LoginDisplayHostMojo
// instead.
void SwitchWebUItoMojo();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
