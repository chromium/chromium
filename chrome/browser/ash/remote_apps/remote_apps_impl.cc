// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr char kErrNotReady[] = "Manager for remote apps is not ready";
constexpr char kErrFolderIdDoesNotExist[] = "Folder ID provided does not exist";
constexpr char kErrAppIdDoesNotExist[] = "App ID provided does not exist";

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
  app_launch_observers_.set_disconnect_handler(base::BindRepeating(
      &RemoteAppsImpl::DisconnectHandler, base::Unretained(this)));
}

RemoteAppsImpl::~RemoteAppsImpl() = default;

void RemoteAppsImpl::Bind(
    const std::string& source_id,
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteApps>
        pending_remote_apps,
    mojo::PendingRemote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
        pending_observer) {
  receivers_.Add(this, std::move(pending_remote_apps));
  const mojo::RemoteSetElementId& remote_id = app_launch_observers_.Add(
      mojo::Remote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>(
          std::move(pending_observer)));
  source_id_to_remote_id_map_.insert(
      std::pair<std::string, mojo::RemoteSetElementId>(source_id, remote_id));
}

void RemoteAppsImpl::AddFolder(const std::string& name,
                               bool add_to_front,
                               AddFolderCallback callback) {
  const std::string& folder_id = manager_->AddFolder(name, add_to_front);
  std::move(callback).Run(
      chromeos::remote_apps::mojom::AddFolderResult::NewFolderId(folder_id));
}

void RemoteAppsImpl::AddApp(const std::string& source_id,
                            const std::string& name,
                            const std::string& folder_id,
                            const GURL& icon_url,
                            bool add_to_front,
                            AddAppCallback callback) {
  manager_->AddApp(
      source_id, name, folder_id, icon_url, add_to_front,
      base::BindOnce(&RemoteAppsImpl::OnAppAdded, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void RemoteAppsImpl::DeleteApp(const std::string& app_id,
                               DeleteAppCallback callback) {
  ash::RemoteAppsError error = manager_->DeleteApp(app_id);

  switch (error) {
    case RemoteAppsError::kNotReady:
      std::move(callback).Run(kErrNotReady);
      return;
    case RemoteAppsError::kNone:
      std::move(callback).Run(absl::nullopt);
      return;
    case RemoteAppsError::kAppIdDoesNotExist:
      std::move(callback).Run(kErrAppIdDoesNotExist);
      return;
    case RemoteAppsError::kFolderIdDoesNotExist:
      // Impossible to reach - only occurs for |AddApp()|.
      DCHECK(false);
  }
}

void RemoteAppsImpl::OnAppLaunched(const std::string& source_id,
                                   const std::string& app_id) {
  auto it = source_id_to_remote_id_map_.find(source_id);
  if (it == source_id_to_remote_id_map_.end())
    return;

  chromeos::remote_apps::mojom::RemoteAppLaunchObserver* observer =
      app_launch_observers_.Get(it->second);
  if (!observer)
    return;

  observer->OnRemoteAppLaunched(app_id);
}

void RemoteAppsImpl::OnAppAdded(AddAppCallback callback,
                                const std::string& app_id,
                                RemoteAppsError error) {
  switch (error) {
    case RemoteAppsError::kNotReady:
      std::move(callback).Run(
          chromeos::remote_apps::mojom::AddAppResult::NewError(kErrNotReady));
      return;
    case RemoteAppsError::kFolderIdDoesNotExist:
      std::move(callback).Run(
          chromeos::remote_apps::mojom::AddAppResult::NewError(
              kErrFolderIdDoesNotExist));
      return;
    case RemoteAppsError::kNone:
      std::move(callback).Run(
          chromeos::remote_apps::mojom::AddAppResult::NewAppId(app_id));
      return;
    case RemoteAppsError::kAppIdDoesNotExist:
      // Impossible to reach - only occurs for |DeleteApp()|.
      DCHECK(false);
  }
}

void RemoteAppsImpl::DisconnectHandler(mojo::RemoteSetElementId id) {
  const auto& it = std::find_if(
      source_id_to_remote_id_map_.begin(), source_id_to_remote_id_map_.end(),
      [&id](const std::pair<std::string, mojo::RemoteSetElementId>& pair) {
        return pair.second == id;
      });

  if (it == source_id_to_remote_id_map_.end())
    return;

  source_id_to_remote_id_map_.erase(it);
}

}  // namespace ash
