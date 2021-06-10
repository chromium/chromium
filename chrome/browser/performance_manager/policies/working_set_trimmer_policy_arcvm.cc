// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"

#include "ash/public/cpp/app_types_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {
namespace policies {
namespace {

content::BrowserContext* GetContext() {
  // For production, always use the primary user profile. ARCVM does not
  // support non-primary profiles. |g_browser_process| can be nullptr during
  // browser shutdown.
  if (g_browser_process && g_browser_process->profile_manager())
    return g_browser_process->profile_manager()->GetPrimaryUserProfile();
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<WorkingSetTrimmerPolicyArcVm>
WorkingSetTrimmerPolicyArcVm::CreateForTesting(
    content::BrowserContext* context) {
  auto* policy = new WorkingSetTrimmerPolicyArcVm();
  policy->context_for_testing_ = context;
  return base::WrapUnique(policy);
}

// static
WorkingSetTrimmerPolicyArcVm* WorkingSetTrimmerPolicyArcVm::Get() {
  static base::NoDestructor<WorkingSetTrimmerPolicyArcVm> arcvm_policy;
  return arcvm_policy.get();
}

WorkingSetTrimmerPolicyArcVm::WorkingSetTrimmerPolicyArcVm() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(arc::IsArcVmEnabled()) << "This is only for ARCVM builds";

  // Ask SessionManager to notify when the user has signed in. In case the user
  // has already signed in, call OnUserSessionStarted() now.
  if (session_manager::SessionManager::Get()->IsSessionStarted())
    OnUserSessionStarted(/*is_primary_user=*/true);
  else
    session_manager::SessionManager::Get()->AddObserver(this);

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);

  if (exo::WMHelper::HasInstance()) {  // for unit tests
    auto* helper = exo::WMHelper::GetInstance();
    helper->AddActivationObserver(this);
    OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        helper->GetActiveWindow(), /*lost_active=*/nullptr);
  }
}

WorkingSetTrimmerPolicyArcVm::~WorkingSetTrimmerPolicyArcVm() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_ : GetContext();
  if (context) {
    auto* metrics_service =
        arc::ArcMetricsService::GetForBrowserContext(context);
    if (metrics_service)
      metrics_service->RemoveUserInteractionObserver(this);

    auto* boot_phase_monitor_bridge =
        arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
    if (boot_phase_monitor_bridge)
      boot_phase_monitor_bridge->RemoveObserver(this);
  }

  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);

  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager)
    session_manager->RemoveObserver(this);
}

bool WorkingSetTrimmerPolicyArcVm::IsEligibleForReclaim(
    const base::TimeDelta& arcvm_inactivity_time) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const bool is_inactive =
      (base::TimeTicks::Now() - last_user_interaction_) > arcvm_inactivity_time;
  return is_boot_complete_ && !is_focused_ && is_inactive;
}

void WorkingSetTrimmerPolicyArcVm::OnBootCompleted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_boot_complete_ = true;
  // Now the user is able to interact with ARCVM. Reset the value.
  last_user_interaction_ = base::TimeTicks::Now();
}

void WorkingSetTrimmerPolicyArcVm::OnUserInteraction(
    arc::UserInteractionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_user_interaction_ = base::TimeTicks::Now();
}

void WorkingSetTrimmerPolicyArcVm::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_boot_complete_ = false;
}
void WorkingSetTrimmerPolicyArcVm::OnArcSessionRestarting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_boot_complete_ = false;
}

void WorkingSetTrimmerPolicyArcVm::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool was_focused = is_focused_;
  is_focused_ = ash::IsArcWindow(gained_active);
  if (was_focused && !is_focused_) {
    // While the window was focused, the user was interacting with ARCVM.
    last_user_interaction_ = base::TimeTicks::Now();
  }
}

void WorkingSetTrimmerPolicyArcVm::OnUserSessionStarted(bool is_primary_user) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!is_primary_user)
    return;

  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_ : GetContext();
  DCHECK(context);

  // ArcBootPhaseMonitorBridge and ArcMetricsService are created when the
  // primary user profile is created. In OnUserSessionStarted(), they always
  // exist.
  auto* boot_phase_monitor_bridge =
      arc::ArcBootPhaseMonitorBridge::GetForBrowserContext(context);
  DCHECK(boot_phase_monitor_bridge);
  boot_phase_monitor_bridge->AddObserver(this);

  auto* metrics_service = arc::ArcMetricsService::GetForBrowserContext(context);
  DCHECK(metrics_service);
  metrics_service->AddUserInteractionObserver(this);
}

}  // namespace policies
}  // namespace performance_manager
