// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_manager.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace chromeos {

namespace {

constexpr char kErrNotReady[] = "Manager for remote apps is not ready";
constexpr char kErrFolderIdDoesNotExist[] = "Folder ID provided does not exist";

static bool g_bypass_checks_for_testing_ = false;

}  // namespace

// static
bool RemoteAppsImpl::IsAllowed(content::RenderFrameHost* render_frame_host,
                               const extensions::Extension* extension) {
  if (!render_frame_host || !extension)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  DCHECK(profile);
  // RemoteApps are not available for non-managed guest sessions.
  if (!RemoteAppsManagerFactory::GetForProfile(profile))
    return false;

  if (g_bypass_checks_for_testing_)
    return true;

  const extensions::Feature* feature =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kImprivataInSessionExtension);
  DCHECK(feature);
  return feature->IsAvailableToExtension(extension).is_available();
}

// static
void RemoteAppsImpl::SetBypassChecksForTesting(bool bypass_checks_for_testing) {
  g_bypass_checks_for_testing_ = bypass_checks_for_testing;
}

RemoteAppsImpl::RemoteAppsImpl(RemoteAppsManager* manager) : manager_(manager) {
  DCHECK(manager);
}

RemoteAppsImpl::~RemoteAppsImpl() = default;

void RemoteAppsImpl::Bind(
    mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
    mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
        pending_observer) {
  receivers_.Add(this, std::move(pending_remote_apps));
  app_launch_observers_.Add(
      mojo::Remote<remote_apps::mojom::RemoteAppLaunchObserver>(
          std::move(pending_observer)));
}

void RemoteAppsImpl::AddFolder(const std::string& name,
                               bool add_to_front,
                               AddFolderCallback callback) {
  const std::string& folder_id = manager_->AddFolder(name, add_to_front);
  std::move(callback).Run(folder_id, base::nullopt);
}

void RemoteAppsImpl::AddApp(const std::string& name,
                            const std::string& folder_id,
                            const GURL& icon_url,
                            bool add_to_front,
                            AddAppCallback callback) {
  manager_->AddApp(
      name, folder_id, icon_url, add_to_front,
      base::BindOnce(&RemoteAppsImpl::OnAppAdded, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void RemoteAppsImpl::OnAppLaunched(const std::string& id) {
  if (!base::Contains(app_ids_, id))
    return;
  for (auto& observer : app_launch_observers_)
    observer->OnRemoteAppLaunched(id);
}

void RemoteAppsImpl::OnAppAdded(AddAppCallback callback,
                                const std::string& id,
                                RemoteAppsError error) {
  switch (error) {
    case RemoteAppsError::kNotReady:
      std::move(callback).Run(base::nullopt, kErrNotReady);
      return;
    case RemoteAppsError::kFolderIdDoesNotExist:
      std::move(callback).Run(base::nullopt, kErrFolderIdDoesNotExist);
      return;
    case RemoteAppsError::kNone:
      app_ids_.insert(id);
      std::move(callback).Run(id, base::nullopt);
      return;
    case RemoteAppsError::kAppIdDoesNotExist:
      // Only occurs when deleting an app, which is not yet implemented in the
      // API.
      DCHECK(false);
  }
}

}  // namespace chromeos
