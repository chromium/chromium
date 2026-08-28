// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_critical_action_logger.h"

#include "base/json/json_writer.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/critical_actions/core/browser/features.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace password_manager {

PasswordManagerCriticalActionLogger::PasswordManagerCriticalActionLogger(
    content::WebContents* web_contents,
    Profile* profile)
    : content::WebContentsObserver(web_contents), profile_(profile) {}

PasswordManagerCriticalActionLogger::~PasswordManagerCriticalActionLogger() =
    default;

void PasswordManagerCriticalActionLogger::MaybeLogCriticalAction(
    PasswordManagerDriver* driver,
    const GURL& url,
    PasswordManagerClient::PasswordFillTrigger trigger_type) {
  if (!base::FeatureList::IsEnabled(
          critical_actions::features::kCriticalActionHistory)) {
    return;
  }

  critical_actions::CriticalActionService* service =
      critical_actions::CriticalActionFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  critical_actions::CriticalActionEntry entry;
  entry.critical_action_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  entry.timestamp = base::Time::Now();
  entry.action_type = critical_actions::ActionType::kFormFill;
  entry.action_source = critical_actions::ActionSource::kPasswordManager;
  entry.url = url;

  std::string type =
      trigger_type == PasswordManagerClient::PasswordFillTrigger::kAgentTask
          ? "agent_task"
          : "password_manager_autofill";

  base::DictValue metadata_dict;
  metadata_dict.Set("type", type);
  std::string metadata_json;
  if (base::JSONWriter::Write(metadata_dict, &metadata_json)) {
    entry.metadata = std::move(metadata_json);
  }

  service->AddCriticalActionWithNavigationId(entry,
                                             GetNavigationIdForDriver(driver));
}

void PasswordManagerCriticalActionLogger::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted()) {
    if (auto* service =
            critical_actions::CriticalActionFactory::GetForProfile(profile_)) {
      service->OnNavigationDiscarded(navigation_handle->GetNavigationId());
    }
  }
}

void PasswordManagerCriticalActionLogger::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    if (int64_t nav_id = render_frame_host->GetNavigationId()) {
      if (auto* service =
              critical_actions::CriticalActionFactory::GetForProfile(
                  profile_)) {
        service->OnNavigationDiscarded(nav_id);
      }
    }
  }
}

int64_t PasswordManagerCriticalActionLogger::GetNavigationIdForDriver(
    PasswordManagerDriver* driver) const {
  content::RenderFrameHost* rfh = nullptr;
  if (driver) {
    rfh =
        static_cast<ContentPasswordManagerDriver*>(driver)->render_frame_host();
  } else if (web_contents()) {
    rfh = web_contents()->GetPrimaryMainFrame();
  }

  // Verify the frame is active (i.e. not prerendered, not in bfcache, and not
  // detached) before returning the navigation ID of its outermost main
  // frame.
  if (rfh && rfh->IsActive()) {
    return rfh->GetOutermostMainFrame()->GetNavigationId();
  }
  return 0;
}

}  // namespace password_manager
