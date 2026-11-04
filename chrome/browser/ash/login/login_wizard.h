// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"

class ApplicationLocaleStorage;
class PrefService;

namespace component_updater {
class ComponentManagerAsh;
}  // namespace component_updater

namespace metrics {
class MetricsService;
}  // namespace metrics

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {
class BrowserPolicyConnectorAsh;
class DeviceRestrictionScheduleController;
}  // namespace policy

namespace user_manager {
class MultiUserSignInPolicyController;
}  // namespace user_manager

namespace ash {

namespace system {
class SystemClock;
}  // namespace system

// Shows the Chrome OS out-of-box / login UI. The LoginDisplayHost may be
// created asynchronously, after a locale switch.
// `local_state`, `metrics_service`, `application_locale_storage`,
// `browser_policy_connector_ash`, `device_restriction_schedule_controller`,
// `multi_user_sign_in_policy_controller` and `system_clock` must be non-null
// and must outlive the LoginDisplayHost that this function creates.
// `shared_url_loader_factory` and `component_manager_ash` must be non-null.
void ShowLoginWizard(
    PrefService* local_state,
    ::metrics::MetricsService* metrics_service,
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    scoped_refptr<component_updater::ComponentManagerAsh> component_manager_ash,
    policy::DeviceRestrictionScheduleController*
        device_restriction_schedule_controller,
    const user_manager::MultiUserSignInPolicyController*
        multi_user_sign_in_policy_controller,
    system::SystemClock* system_clock,
    OobeScreenId first_screen);

// Shuts down LoginDisplayHostWebUI host and creates LoginDisplayHostMojo
// instead. This may happen asynchronously, after a locale switch.
// `local_state`, `metrics_service`, `application_locale_storage`,
// `browser_policy_connector_ash`, `device_restriction_schedule_controller`,
// `multi_user_sign_in_policy_controller` and `system_clock` must be non-null
// and must outlive the LoginDisplayHost that this function creates.
// `shared_url_loader_factory` and `component_manager_ash` must be non-null.
void SwitchWebUItoMojo(
    PrefService* local_state,
    ::metrics::MetricsService* metrics_service,
    ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    policy::BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    scoped_refptr<component_updater::ComponentManagerAsh> component_manager_ash,
    policy::DeviceRestrictionScheduleController*
        device_restriction_schedule_controller,
    const user_manager::MultiUserSignInPolicyController*
        multi_user_sign_in_policy_controller,
    system::SystemClock* system_clock);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
