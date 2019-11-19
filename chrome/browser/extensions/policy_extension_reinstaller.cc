// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_extension_reinstaller.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

namespace {

PolicyExtensionReinstaller::ReinstallCallback* g_reinstall_action_for_test =
    nullptr;

const net::BackoffEntry::Policy kPolicyReinstallBackoffPolicy = {
    // num_errors_to_ignore
    1,

    // initial_delay_ms (note that we set 'always_use_initial_delay' to false
    // below)
    100,

    // multiply_factor
    2,

    // jitter_factor
    0.1,

    // maximum_backoff_ms (30 minutes)
    1000 * 60 * 30,

    // entry_lifetime_ms (6 hours)
    1000 * 60 * 60 * 6,

    // always_use_initial_delay
    false,
};

}  // namespace

PolicyExtensionReinstaller::PolicyExtensionReinstaller(
    content::BrowserContext* context)
    : context_(context), backoff_entry_(&kPolicyReinstallBackoffPolicy) {}

PolicyExtensionReinstaller::~PolicyExtensionReinstaller() {}

// static
void PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(
    ReinstallCallback* action) {
  g_reinstall_action_for_test = action;
}

void PolicyExtensionReinstaller::NotifyExtensionDisabledDueToCorruption() {
  ScheduleNextReinstallAttempt();
}

void PolicyExtensionReinstaller::Fire() {
  scheduled_fire_pending_ = false;
  ExtensionSystem* system = ExtensionSystem::Get(context_);
  ExtensionService* service = system->extension_service();
  PendingExtensionManager* pending_manager =
      service->pending_extension_manager();
  // If there's nothing to repair, then bail out.
  if (!pending_manager->HasAnyPolicyReinstallForCorruption())
    return;

  service->CheckForExternalUpdates();
  ScheduleNextReinstallAttempt();
}

base::TimeDelta PolicyExtensionReinstaller::GetNextFireDelay() {
  backoff_entry_.InformOfRequest(false);
  return backoff_entry_.GetTimeUntilRelease();
}

void PolicyExtensionReinstaller::ScheduleNextReinstallAttempt() {
  if (scheduled_fire_pending_)
    return;

  scheduled_fire_pending_ = true;
  base::TimeDelta reinstall_delay = GetNextFireDelay();
  base::OnceClosure callback = base::BindOnce(&PolicyExtensionReinstaller::Fire,
                                              weak_factory_.GetWeakPtr());
  if (g_reinstall_action_for_test) {
    g_reinstall_action_for_test->Run(std::move(callback), reinstall_delay);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, std::move(callback), reinstall_delay);
  }
}

}  // namespace extensions
