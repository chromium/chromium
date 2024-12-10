// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/birch/coral_util.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/coral/fake_coral_service.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "base/command_line.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr int kMinItemsInGroup = 4;
constexpr int kMaxItemsInGroup = 10;
constexpr int kMaxGroupsToGenerate = 2;
// Too many items in 1 request could result in poor performance.
constexpr size_t kMaxItemsInRequest = 100;
constexpr base::TimeDelta kRequestCoralServiceTimeout = base::Seconds(10);

aura::Window* FindAppWindowOnActiveDesk(const std::string& app_id) {
  for (const auto& window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    // Skip invalid windows.
    if (!coral_util::CanMoveToNewDesk(window)) {
      continue;
    }

    const std::string* app_id_key = window->GetProperty(kAppIDKey);
    if (app_id_key && *app_id_key == app_id) {
      return window;
    }
  }

  return nullptr;
}

const char* SourceToString(CoralSource source) {
  switch (source) {
    case CoralSource::kInSession:
      return "in-session";
    case CoralSource::kPostLogin:
      return "post-login";
    case CoralSource::kUnknown:
      return "unknown";
  }
}

std::string GroupResponseToString(
    const coral::mojom::GroupResponsePtr& group_response) {
  const std::vector<coral::mojom::GroupPtr>& groups = group_response->groups;
  std::string group_info = base::NumberToString(groups.size()) + " groups";
  for (size_t i = 0; i < groups.size(); i++) {
    group_info += ", group " + base::NumberToString(i + 1) + " has " +
                  base::NumberToString(groups[i]->entities.size()) +
                  " entities";
  }
  return group_info;
}

}  // namespace

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

std::string CoralRequest::ToString() const {
  auto root = base::Value::Dict().Set(
      "Coral request", coral_util::EntitiesToListValue(content_));
  return root.DebugString();
}

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

CoralController::CoralController() = default;

CoralController::~CoralController() = default;

void CoralController::PrepareResource() {
  CoralService* coral_service = EnsureCoralService();
  if (!coral_service) {
    LOG(ERROR) << "Failed to connect to coral service.";
    return;
  }
  coral_service->PrepareResource();
}

void CoralController::GenerateContentGroups(
    const CoralRequest& request,
    mojo::PendingRemote<coral::mojom::TitleObserver> title_observer,
    CoralResponseCallback callback) {
  // There couldn't be valid groups, skip generating and return an empty
  // response.
  if (request.content().size() < kMinItemsInGroup) {
    std::move(callback).Run(std::make_unique<CoralResponse>());
    return;
  }

  CoralService* coral_service = EnsureCoralService();
  if (!coral_service) {
    LOG(ERROR) << "Failed to connect to coral service.";
    std::move(callback).Run(nullptr);
    return;
  }

  auto group_request = coral::mojom::GroupRequest::New();
  group_request->embedding_options = coral::mojom::EmbeddingOptions::New();
  group_request->clustering_options = coral::mojom::ClusteringOptions::New();
  group_request->clustering_options->min_items_in_cluster = kMinItemsInGroup;
  group_request->clustering_options->max_items_in_cluster = kMaxItemsInGroup;
  group_request->clustering_options->max_clusters = kMaxGroupsToGenerate;
  group_request->title_generation_options =
      coral::mojom::TitleGenerationOptions::New();
  const size_t items_in_request =
      std::min(request.content().size(), kMaxItemsInRequest);
  for (size_t i = 0; i < items_in_request; i++) {
    group_request->entities.push_back(request.content()[i]->Clone());
  }
  coral_service->Group(
      std::move(group_request), std::move(title_observer),
      base::BindOnce(&CoralController::HandleGroupResult,
                     weak_factory_.GetWeakPtr(), request.source(),
                     std::move(callback), base::TimeTicks::Now()));
}

void CoralController::CacheEmbeddings(const CoralRequest& request,
                                      base::OnceCallback<void(bool)> callback) {
  CoralService* coral_service = EnsureCoralService();
  if (!coral_service) {
    LOG(ERROR) << "Failed to connect to coral service.";
    std::move(callback).Run(false);
    return;
  }

  auto cache_embeddings_request = coral::mojom::CacheEmbeddingsRequest::New();
  cache_embeddings_request->embedding_options =
      coral::mojom::EmbeddingOptions::New();
  for (const auto& entity : request.content()) {
    cache_embeddings_request->entities.push_back(entity->Clone());
  }

  coral_service->CacheEmbeddings(
      std::move(cache_embeddings_request),
      base::BindOnce(&CoralController::HandleCacheEmbeddingsResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CoralController::OpenNewDeskWithGroup(CoralResponse::Group group,
                                           const Desk* source_desk) {
  CHECK(!!source_desk);
  if (group->entities.empty()) {
    return;
  }

  DesksController* desks_controller = DesksController::Get();
  CHECK(desks_controller->CanCreateDesks());
  const coral_util::TabsAndApps tabs_apps =
      coral_util::SplitContentData(group->entities);

  int src_desk_index = desks_controller->GetDeskIndex(source_desk);

  // First place all windows that should be moved in a set, this is so we can
  // have O(1) lookups for snap groups later.
  base::flat_set<aura::Window*> windows_set;
  for (const auto& app : tabs_apps.apps) {
    if (aura::Window* window = FindAppWindowOnActiveDesk(app.id)) {
      windows_set.insert(window);
    }
  }

  // Create the new desk to which the apps and tabs will be moved. And activate
  // it.
  Desk* new_desk = desks_controller->CreateNewDeskForCoralGroup(
      base::UTF8ToUTF16(group->title.value_or(std::string())));

  auto* snap_group_controller = SnapGroupController::Get();
  {
    // Don't update desk mini view of the `new_desk` until all the apps and tabs
    // are moved to the `new_desk`.
    auto new_desk_mini_view_pauser =
        Desk::ScopedContentUpdateNotificationDisabler(
            /*desks=*/{new_desk},
            /*notify_when_destroyed=*/true);

    for (aura::Window* window : windows_set) {
      // If a window is part of a snap group, and the other window is not part
      // of `windows_set` (i.e. not in the group), remove the snap group first
      // otherwise both windows will be moved.
      if (SnapGroup* snap_group =
              snap_group_controller->GetSnapGroupForGivenWindow(window)) {
        aura::Window* other_window = window == snap_group->window1()
                                         ? snap_group->window2()
                                         : snap_group->window1();
        CHECK(other_window);
        if (!windows_set.contains(other_window)) {
          snap_group_controller->RemoveSnapGroup(snap_group,
                                                 SnapGroupExitPoint::kCoral);
        }
      }

      desks_controller->MoveWindowFromDeskAtIndexTo(
          window, src_desk_index, new_desk, window->GetRootWindow(),
          DesksMoveWindowFromActiveDeskSource::kCoral);
    }

    // Move tabs to a browser on the new desk.
    Shell::Get()->coral_delegate()->MoveTabsInGroupToNewDesk(tabs_apps.tabs,
                                                             src_desk_index);
  }
}

void CoralController::CreateSavedDeskFromGroup(coral::mojom::GroupPtr group) {
  std::vector<GURL> tab_urls;
  base::flat_set<std::string> app_ids;
  for (const coral::mojom::EntityPtr& entity : group->entities) {
    if (entity->is_tab()) {
      tab_urls.push_back(entity->get_tab()->url);
    } else if (entity->is_app()) {
      app_ids.insert(entity->get_app()->id);
    }
  }

  // `RestoreDataCollector` has a callback because it is compatible with lacros
  // which needs to be async. If there are no apps and just tabs, we can
  // directly trigger the callback, but we have to create the template
  // ourselves.
  if (app_ids.empty()) {
    OnTemplateCreated(tab_urls, /*desk_template=*/nullptr);
    return;
  }

  // TODO(crbug.com/365839564): Handle multi display case.
  DesksController::Get()->CaptureActiveDeskAsSavedDesk(
      base::BindOnce(&CoralController::OnTemplateCreated,
                     weak_factory_.GetWeakPtr(), tab_urls),
      DeskTemplateType::kCoral,
      /*root_window_to_show=*/Shell::GetPrimaryRootWindow(), app_ids);
}

CoralController::CoralService* CoralController::EnsureCoralService() {
  // Generate a fake service if --force-birch-fake-coral-backend is enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoralBackend)) {
    if (!fake_service_) {
      fake_service_ = std::make_unique<FakeCoralService>();
    }
    return fake_service_.get();
  }

  if (!coral_service_ && mojo_service_manager::IsServiceManagerBound()) {
    auto pipe_handle = coral_service_.BindNewPipeAndPassReceiver().PassPipe();
    coral_service_.reset_on_disconnect();
    mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosCoralService, kRequestCoralServiceTimeout,
        std::move(pipe_handle));
  }
  return coral_service_ ? coral_service_.get() : nullptr;
}

void CoralController::HandleGroupResult(CoralSource source,
                                        CoralResponseCallback callback,
                                        const base::TimeTicks& request_time,
                                        coral::mojom::GroupResultPtr result) {
  if (result->is_error()) {
    LOG(ERROR) << "Coral group request failed with CoralError code: "
               << static_cast<int>(result->get_error());
    std::move(callback).Run(nullptr);
    return;
  }
  coral::mojom::GroupResponsePtr group_response =
      std::move(result->get_response());
  VLOG(1) << "Coral group " << SourceToString(source)
          << " request succeeded with " << GroupResponseToString(group_response)
          << ", in " << (base::TimeTicks::Now() - request_time).InMilliseconds()
          << " ms.";
  auto response = std::make_unique<CoralResponse>();
  response->set_source(source);
  response->set_groups(std::move(group_response->groups));
  std::move(callback).Run(std::move(response));
}

void CoralController::HandleCacheEmbeddingsResult(
    base::OnceCallback<void(bool)> callback,
    coral::mojom::CacheEmbeddingsResultPtr result) {
  if (result->is_error()) {
    LOG(ERROR) << "Coral cache embeddings request failed with CoralError code: "
               << static_cast<int>(result->get_error());
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
}

void CoralController::OnTemplateCreated(
    const std::vector<GURL>& tab_urls,
    std::unique_ptr<DeskTemplate> desk_template) {
  std::unique_ptr<DeskTemplate> new_template = std::move(desk_template);
  // There is a chance the template is empty due to unsupported apps.
  if (!new_template) {
    if (tab_urls.empty()) {
      return;
    }

    // TODO(crbug.com/365839564): The title can be nullopt and updated async
    // after. Figure out how to handle that case.
    new_template = std::make_unique<DeskTemplate>(
        base::Uuid::GenerateRandomV4(), DeskTemplateSource::kUser,
        "saved group", base::Time::Now(), DeskTemplateType::kCoral);
    new_template->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
  }

  auto* shell = Shell::Get();
  if (!tab_urls.empty()) {
    auto* restore_data = new_template->mutable_desk_restore_data();
    auto& launch_list =
        restore_data
            ->mutable_app_id_to_launch_list()[app_constants::kChromeAppId];
    // All tabs go into the same window.
    auto& app_restore_data =
        launch_list[shell->coral_delegate()->GetChromeDefaultRestoreId()];
    app_restore_data = std::make_unique<app_restore::AppRestoreData>();
    app_restore_data->browser_extra_info.urls = std::move(tab_urls);
  }

  // TODO(crbug.com/365839564): Callback should show the templates library view.
  shell->saved_desk_delegate()->GetDeskModel()->AddOrUpdateEntry(
      std::move(new_template), base::DoNothing());
}

}  // namespace ash
