// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/working_set_trimmer_policy_arcvm.h"

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/public/cpp/app_types_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager {
namespace policies {
namespace {

// The delay until |is_boot_complete_and_connected_| is flipped after app.mojom
// and intent_helper.mojom are connected, The delay is necessary because when
// the connections are established, ARCVM is often still busy accessing many
// files. The current value (100s) is based on the slowest SKU (Bluebird) of the
// Octopus family. It usually takes about 50 seconds for the device to become
// idle after the mojo connections ready event, and the 100s setting is 50s * 2
// (safety factor) considering that we might launch ARCVM on (slightly) slower
// devices than Octopus in the future.
constexpr base::TimeDelta kArcVmBootDelay = base::Seconds(100);

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

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  // ArcSessionManager is created very early in
  // `ChromeBrowserMainPartsAsh::PreMainMessageLoopRun()`.
  DCHECK(arc_session_manager);
  arc_session_manager->AddObserver(this);

  if (exo::WMHelper::HasInstance()) {  // for unit tests
    auto* helper = exo::WMHelper::GetInstance();
    helper->AddActivationObserver(this);
    OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        helper->GetActiveWindow(), /*lost_active=*/nullptr);
  }

  // If app() and/or intent_helper() are already connected to the instance in
  // the guest, the OnConnectionReady() function is synchronously called before
  // returning from AddObserver. For more details, see
  // ash/components/arc/session/connection_holder.h, especially its
  // AddObserver() function.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  // ArcServiceManager and objects owned by the manager are created very early
  // in `ChromeBrowserMainPartsAsh::PreMainMessageLoopRun()` too.
  DCHECK(arc_service_manager);
  arc_service_manager->arc_bridge_service()->app()->AddObserver(this);
  arc_service_manager->arc_bridge_service()->intent_helper()->AddObserver(this);
}

WorkingSetTrimmerPolicyArcVm::~WorkingSetTrimmerPolicyArcVm() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_.get() : GetContext();
  if (context) {
    auto* metrics_service =
        arc::ArcMetricsService::GetForBrowserContext(context);
    if (metrics_service)
      metrics_service->RemoveUserInteractionObserver(this);
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc_service_manager->arc_bridge_service()->intent_helper()->RemoveObserver(
      this);
  arc_service_manager->arc_bridge_service()->app()->RemoveObserver(this);

  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);

  auto* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager)
    arc_session_manager->RemoveObserver(this);
}

mechanism::ArcVmReclaimType WorkingSetTrimmerPolicyArcVm::IsEligibleForReclaim(
    const base::TimeDelta& arcvm_inactivity_time,
    mechanism::ArcVmReclaimType trim_once_type_after_arcvm_boot,
    bool* is_first_trim_post_boot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_first_trim_post_boot)
    *is_first_trim_post_boot = false;  // Unless proven otherwise, below.
  if (!is_boot_complete_and_connected_)
    return mechanism::ArcVmReclaimType::kReclaimNone;
  if (!trimmed_at_boot_ && trim_once_type_after_arcvm_boot !=
                               mechanism::ArcVmReclaimType::kReclaimNone) {
    if (is_first_trim_post_boot)
      *is_first_trim_post_boot = true;
    trimmed_at_boot_ = true;
    return trim_once_type_after_arcvm_boot;
  }
  const bool is_inactive =
      (base::TimeTicks::Now() - last_user_interaction_) > arcvm_inactivity_time;
  return (!is_focused_ && is_inactive)
             ? mechanism::ArcVmReclaimType::kReclaimAll
             : mechanism::ArcVmReclaimType::kReclaimNone;
}

void WorkingSetTrimmerPolicyArcVm::OnUserInteraction(
    arc::UserInteractionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_user_interaction_ = base::TimeTicks::Now();
}

void WorkingSetTrimmerPolicyArcVm::OnArcSessionStopped(
    arc::ArcStopReason stop_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // OnConnectionReadyInternal() shouldn't be called anymore.
  timer_.Stop();

  is_boot_complete_and_connected_ = false;
  trimmed_at_boot_ = false;
}

void WorkingSetTrimmerPolicyArcVm::OnArcSessionRestarting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // OnConnectionReadyInternal() shouldn't be called anymore.
  timer_.Stop();

  is_boot_complete_and_connected_ = false;
  trimmed_at_boot_ = false;
}

void WorkingSetTrimmerPolicyArcVm::OnConnectionReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Check if both mojo connections are established.
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  const bool app_connected =
      arc_service_manager &&
      arc_service_manager->arc_bridge_service()->app()->IsConnected();
  const bool intent_helper_connected =
      arc_service_manager &&
      arc_service_manager->arc_bridge_service()->intent_helper()->IsConnected();
  if (!app_connected || !intent_helper_connected)
    return;

  // Wait for a while more until ARCVM stops accessing too many files. Using
  // Unretained() is safe here because |this| object is created with
  // base::NoDestructor<> in the factory method above.
  timer_.Start(
      FROM_HERE, kArcVmBootDelay,
      base::BindOnce(&WorkingSetTrimmerPolicyArcVm::OnConnectionReadyInternal,
                     base::Unretained(this)));
}

void WorkingSetTrimmerPolicyArcVm::OnConnectionReadyInternal() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  is_boot_complete_and_connected_ = true;
  // Now the user is able to interact with ARCVM. Reset the value.
  last_user_interaction_ = base::TimeTicks::Now();
  if (!observing_user_interactions_) {
    StartObservingUserInteractions();
    observing_user_interactions_ = true;
  }
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

// static
const base::TimeDelta&
WorkingSetTrimmerPolicyArcVm::GetArcVmBootDelayForTesting() {
  return kArcVmBootDelay;
}

void WorkingSetTrimmerPolicyArcVm::StartObservingUserInteractions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* context =
      context_for_testing_ ? context_for_testing_.get() : GetContext();
  DCHECK(context);

  // ArcMetricsService is created in OnPrimaryUserProfilePrepared() in
  // ArcServiceLauncher which also initializes objects that are needed to start
  // ARCVM e.g. ArcSessionManager. As long as the function is called after ARCVM
  // is started, e.g. from OnConnectionReady(), the DCHECK below should never
  // fail.
  auto* metrics_service = arc::ArcMetricsService::GetForBrowserContext(context);
  DCHECK(metrics_service);
  DCHECK(!observing_user_interactions_);
  metrics_service->AddUserInteractionObserver(this);
}

}  // namespace policies
}  // namespace performance_manager
