// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/public/cpp/coral_delegate.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/json/json_writer.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"

namespace ash {

namespace {
constexpr int kMinItemsInGroup = 4;
constexpr int kMaxItemsInGroup = 10;
constexpr int kMaxGroupsToGenerate = 2;
// Too many items in 1 request could result in poor performance.
constexpr size_t kMaxItemsInRequest = 100;
}  // namespace

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

std::string CoralRequest::ToString() const {
  auto list = base::Value::List();
  for (const ContentItem& item : content_) {
    auto item_value = base::Value::Dict();
    if (item->is_tab()) {
      item_value.Set("Tab", base::Value::Dict()
                                .Set("Title", item->get_tab()->title)
                                .Set("Url", item->get_tab()->url.spec()));
    }
    if (item->is_app()) {
      item_value.Set("App", base::Value::Dict()
                                .Set("Title", item->get_app()->title)
                                .Set("Id", item->get_app()->id));
    }
    list.Append(std::move(item_value));
  }

  auto root = base::Value::Dict().Set("Coral request", std::move(list));
  return base::WriteJsonWithOptions(root,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or(std::string());
}

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

CoralController::CoralController() = default;

CoralController::~CoralController() = default;

void CoralController::GenerateContentGroups(const CoralRequest& request,
                                            CoralResponseCallback callback) {
  // There couldn't be valid groups, skip generating and return an empty
  // response.
  if (request.content().size() < kMinItemsInGroup) {
    std::move(callback).Run(std::make_unique<CoralResponse>());
    return;
  }

  EnsureCoralService();
  if (!coral_service_) {
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
  coral_service_->Group(
      std::move(group_request),
      base::BindOnce(&CoralController::HandleGroupResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CoralController::CacheEmbeddings(const CoralRequest& request,
                                      base::OnceCallback<void(bool)> callback) {
  EnsureCoralService();
  if (!coral_service_) {
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

  coral_service_->CacheEmbeddings(
      std::move(cache_embeddings_request),
      base::BindOnce(&CoralController::HandleCacheEmbeddingsResult,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CoralController::EnsureCoralService() {
  if (coral_service_) {
    return;
  }
  auto pipe_handle = coral_service_.BindNewPipeAndPassReceiver().PassPipe();
  coral_service_.reset_on_disconnect();
  ash::mojo_service_manager::GetServiceManagerProxy()->Request(
      chromeos::mojo_services::kCrosCoralService, std::nullopt,
      std::move(pipe_handle));
}

void CoralController::HandleGroupResult(CoralResponseCallback callback,
                                        coral::mojom::GroupResultPtr result) {
  if (result->is_error()) {
    LOG(ERROR) << "Coral group request failed with CoralError code: "
               << static_cast<int>(result->get_error());
    std::move(callback).Run(nullptr);
    return;
  }
  coral::mojom::GroupResponsePtr group_response =
      std::move(result->get_response());
  auto response = std::make_unique<CoralResponse>();
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
  if (!desks_controller->CanCreateDesks()) {
    return;
  }
  desks_controller->NewDesk(DesksCreationRemovalSource::kCoral,
                            base::UTF8ToUTF16(group->title));
  Shell::Get()->coral_delegate()->MoveTabsInGroupToNewDesk(std::move(group));

  // TODO(zxdan): move the apps in group to the new desk.
  desks_controller->ActivateDesk(desks_controller->desks().back().get(),
                                 DesksSwitchSource::kCoral);
}

}  // namespace ash
