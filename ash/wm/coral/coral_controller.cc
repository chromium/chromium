// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/birch/coral_util.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/coral/fake_coral_service.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "base/command_line.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
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

void CoralController::OpenNewDeskWithGroup(CoralResponse::Group group) {
  if (group->entities.empty()) {
    return;
  }

  DesksController* desks_controller = DesksController::Get();
  CHECK(desks_controller->CanCreateDesks());
  desks_controller->NewDesk(
      DesksCreationRemovalSource::kCoral,
      base::UTF8ToUTF16(group->title.value_or(std::string())));
  const coral_util::TabsAndApps tabs_apps =
      coral_util::SplitContentData(group->entities);

  // Move tabs to a browser on the new desk.
  Shell::Get()->coral_delegate()->MoveTabsInGroupToNewDesk(tabs_apps.tabs);

  // Move the apps to the new desk.
  const int new_desk_idx = desks_controller->GetNumberOfDesks() - 1;
  Desk* new_desk = desks_controller->GetDeskAtIndex(new_desk_idx);

  // First place all windows that should be moved in a set, this is so we can
  // have O(1) lookups for snap groups later.
  base::flat_set<aura::Window*> windows_set;
  for (const auto& app : tabs_apps.apps) {
    if (aura::Window* window = FindAppWindowOnActiveDesk(app.id)) {
      windows_set.insert(window);
    }
  }

  auto* snap_group_controller = SnapGroupController::Get();
  for (aura::Window* window : windows_set) {
    // If a window is part of a snap group, and the other window is not part of
    // `windows_set` (i.e. not in the group), remove the snap group first
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
    desks_controller->MoveWindowFromActiveDeskTo(
        window, new_desk, window->GetRootWindow(),
        DesksMoveWindowFromActiveDeskSource::kCoral);
  }

  desks_controller->ActivateDesk(desks_controller->desks().back().get(),
                                 DesksSwitchSource::kCoral);
}

}  // namespace ash
