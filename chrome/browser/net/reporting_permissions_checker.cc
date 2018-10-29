// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/reporting_permissions_checker.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"
#include "url/origin.h"

ReportingPermissionsChecker::ReportingPermissionsChecker(
    base::WeakPtr<Profile> weak_profile)
    : weak_profile_(weak_profile) {}

ReportingPermissionsChecker::~ReportingPermissionsChecker() = default;

void ReportingPermissionsChecker::FilterReportingOrigins(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &ReportingPermissionsCheckerFactory::DoFilterReportingOrigins,
          weak_profile_, std::move(origins)),
      std::move(result_callback));
}

ReportingPermissionsCheckerFactory::ReportingPermissionsCheckerFactory(
    Profile* profile)
    : weak_factory_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ReportingPermissionsCheckerFactory::~ReportingPermissionsCheckerFactory() =
    default;

std::unique_ptr<ReportingPermissionsChecker>
ReportingPermissionsCheckerFactory::CreateChecker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return std::make_unique<ReportingPermissionsChecker>(
      weak_factory_.GetWeakPtr());
}

// static
std::set<url::Origin>
ReportingPermissionsCheckerFactory::DoFilterReportingOrigins(
    base::WeakPtr<Profile> weak_profile,
    std::set<url::Origin> origins) {
  if (!weak_profile) {
    return std::set<url::Origin>();
  }

  content::PermissionController* permission_controller =
      content::BrowserContext::GetPermissionController(weak_profile.get());
  DCHECK(permission_controller);

  for (auto it = origins.begin(); it != origins.end();) {
    GURL origin = it->GetURL();
    bool allowed = permission_controller->GetPermissionStatus(
                       content::PermissionType::BACKGROUND_SYNC, origin,
                       origin) == blink::mojom::PermissionStatus::GRANTED;
    if (!allowed) {
      origins.erase(it++);
    } else {
      ++it;
    }
  }
  return origins;
}
