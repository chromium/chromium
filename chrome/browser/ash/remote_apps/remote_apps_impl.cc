// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/remote_apps/remote_apps_impl.h"

#include <optional>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ash/remote_apps/remote_apps_types.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/behavior_feature.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"

namespace ash {

namespace {

constexpr char kErrNotReady[] = "Manager for remote apps is not ready";
constexpr char kErrFolderIdDoesNotExist[] = "Folder ID provided does not exist";
constexpr char kErrAppIdDoesNotExist[] = "App ID provided does not exist";
constexpr char kErrFailedToPinAnApp[] =
    "Invalid app ID or corresponding app is already pinned";
constexpr char kErrPinningMultipleAppsNotSupported[] =
    "Pinning multiple apps is not yet supported";

static bool g_bypass_checks_for_testing_ = false;

}  // namespace

// static
bool RemoteAppsImpl::IsMojoPrivateApiAllowed(
    content::RenderFrameHost* render_frame_host,
    const extensions::Extension* extension) {
  if (!render_frame_host || !extension)
    return false;

  if (g_bypass_checks_for_testing_)
    return true;

  const extensions::Feature* feature =
      extensions::FeatureProvider::GetBehaviorFeature(
          extensions::behavior_feature::kImprivataInSessionExtension);
  DCHECK(feature);
  if (!feature->IsAvailableToExtension(extension).is_available())
    return false;

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  DCHECK(profile);
  // RemoteApps are available for managed guest sessions and regular user
  // sessions.
  if (!RemoteAppsManagerFactory::GetForProfile(profile))
    return false;

  return true;
}

// static
void RemoteAppsImpl::SetBypassChecksForTesting(bool bypass_checks_for_testing) {
  g_bypass_checks_for_testing_ = bypass_checks_for_testing;
}

RemoteAppsImpl::RemoteAppsImpl(RemoteAppsManager* manager) : manager_(manager) {
  DCHECK(manager);
  app_launch_observers_with_source_id_.set_disconnect_handler(
      base::BindRepeating(&RemoteAppsImpl::DisconnectHandler,
                          base::Unretained(this)));
}

RemoteAppsImpl::~RemoteAppsImpl() = default;

void RemoteAppsImpl::BindRemoteAppsAndAppLaunchObserver(
    const std::optional<std::string>& source_id,
    mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteApps>
        pending_remote_apps,
    mojo::PendingRemote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
        pending_observer) {
  receivers_.Add(this, std::move(pending_remote_apps));
  if (!source_id) {
    app_launch_broadcast_observers_.Add(std::move(pending_observer));
    return;
  }

  const mojo::RemoteSetElementId& remote_id =
      app_launch_observers_with_source_id_.Add(
          mojo::Remote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>(
              std::move(pending_observer)));
  source_id_to_remote_id_map_.insert(
      std::pair<std::string, mojo::RemoteSetElementId>(*source_id, remote_id));
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
      std::move(callback).Run(std::nullopt);
      return;
    case RemoteAppsError::kAppIdDoesNotExist:
      std::move(callback).Run(kErrAppIdDoesNotExist);
      return;
    case RemoteAppsError::kFolderIdDoesNotExist:
    case RemoteAppsError::kFailedToPinAnApp:
    case RemoteAppsError::kPinningMultipleAppsNotSupported:
      // Errors specific to other methods.
      NOTREACHED_IN_MIGRATION();
  }
}

void RemoteAppsImpl::SortLauncherWithRemoteAppsFirst(
    SortLauncherWithRemoteAppsFirstCallback callback) {
  manager_->SortLauncherWithRemoteAppsFirst();
  std::move(callback).Run(std::nullopt);
}

void RemoteAppsImpl::SetPinnedApps(const std::vector<std::string>& app_ids,
                                   SetPinnedAppsCallback callback) {
  ash::RemoteAppsError error = manager_->SetPinnedApps(app_ids);
  switch (error) {
    case RemoteAppsError::kNone:
      std::move(callback).Run(std::nullopt);
      return;
    case RemoteAppsError::kFailedToPinAnApp:
      std::move(callback).Run(kErrFailedToPinAnApp);
      return;
    case RemoteAppsError::kPinningMultipleAppsNotSupported:
      std::move(callback).Run(kErrPinningMultipleAppsNotSupported);
      return;
    case RemoteAppsError::kAppIdDoesNotExist:
    case RemoteAppsError::kFolderIdDoesNotExist:
    case RemoteAppsError::kNotReady:
      // Errors specific to other methods.
      NOTREACHED_IN_MIGRATION();
  }
}

void RemoteAppsImpl::OnAppLaunched(const std::string& source_id,
                                   const std::string& app_id) {
  // Dispatch events to broadcast observers.
  for (auto& observer : app_launch_broadcast_observers_)
    observer->OnRemoteAppLaunched(app_id, source_id);

  // Find remote associated with `source_id` and dispatch event to it.
  auto it = source_id_to_remote_id_map_.find(source_id);
  if (it == source_id_to_remote_id_map_.end())
    return;

  chromeos::remote_apps::mojom::RemoteAppLaunchObserver* observer =
      app_launch_observers_with_source_id_.Get(it->second);
  if (!observer)
    return;

  observer->OnRemoteAppLaunched(app_id, source_id);
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
    case RemoteAppsError::kFailedToPinAnApp:
    case RemoteAppsError::kPinningMultipleAppsNotSupported:
      // Errors specific to other methods.
      NOTREACHED_IN_MIGRATION();
  }
}

void RemoteAppsImpl::DisconnectHandler(mojo::RemoteSetElementId id) {
  const auto& it = base::ranges::find(source_id_to_remote_id_map_, id,
                                      &SourceToRemoteIds::value_type::second);

  if (it == source_id_to_remote_id_map_.end())
    return;

  source_id_to_remote_id_map_.erase(it);
}

}  // namespace ash
