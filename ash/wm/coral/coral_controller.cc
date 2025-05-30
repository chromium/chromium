// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include <memory>

#include "ash/birch/coral_util.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/coral/fake_coral_processor.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/mojo_service_manager/connection.h"
#include "chromeos/ash/services/coral/public/mojom/coral_service.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/restore_data.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/feedback/feedback_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/cros_system_api/mojo/service_constants.h"
#include "ui/aura/window_tracker.h"

#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr int kMinItemsInGroup = 4;
constexpr int kMaxItemsInGroup = 25;
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

std::string CoralSourceToString(CoralSource source) {
  switch (source) {
    case CoralSource::kUnknown:
      return "Unknown";
    case CoralSource::kPostLogin:
      return "Post-login";
    case CoralSource::kInSession:
      return "In-session";
  }
}

}  // namespace

CoralRequest::CoralRequest() = default;

CoralRequest::~CoralRequest() = default;

std::string CoralRequest::ToString() const {
  auto request_value = base::Value::Dict();
  request_value.Set("source", CoralSourceToString(source_));
  request_value.Set("requested entities",
                    coral_util::EntitiesToListValue(content_));
  request_value.Set("suppression context",
                    coral_util::EntitiesToListValue(suppression_context_));
  request_value.Set("language", language_);
  auto root =
      base::Value::Dict().Set("Coral request", std::move(request_value));
  return root.DebugString();
}

CoralResponse::CoralResponse() = default;

CoralResponse::~CoralResponse() = default;

CoralController::CoralController() = default;

CoralController::~CoralController() = default;

void CoralController::Initialize(std::string language) {
  CoralProcessor* coral_processor = EnsureCoralProcessor(std::move(language));
  if (!coral_processor) {
    LOG(ERROR) << "Failed to connect to coral processor.";
  }
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

  CoralProcessor* coral_processor = EnsureCoralProcessor(request.language());
  if (!coral_processor) {
    LOG(ERROR) << "Failed to connect to coral processor.";
    std::move(callback).Run(nullptr);
    return;
  }

  auto group_request = coral::mojom::GroupRequest::New();
  group_request->embedding_options = coral::mojom::EmbeddingOptions::New();
  group_request->embedding_options->check_safety_filter = true;
  group_request->clustering_options = coral::mojom::ClusteringOptions::New();
  group_request->clustering_options->min_items_in_cluster = kMinItemsInGroup;
  group_request->clustering_options->max_items_in_cluster = kMaxItemsInGroup;
  group_request->clustering_options->max_clusters = kMaxGroupsToGenerate;
  group_request->title_generation_options =
      coral::mojom::TitleGenerationOptions::New();
  group_request->title_generation_options->language_code = request.language();
  const size_t items_in_request =
      std::min(request.content().size(), kMaxItemsInRequest);
  for (size_t i = 0; i < items_in_request; i++) {
    group_request->entities.push_back(request.content()[i]->Clone());
  }

  if (!request.suppression_context().empty()) {
    group_request->suppression_context =
        mojo::Clone(request.suppression_context());
  }
  coral_processor->Group(
      std::move(group_request), std::move(title_observer),
      base::BindOnce(&CoralController::HandleGroupResult,
                     weak_factory_.GetWeakPtr(), request.source(),
                     std::move(callback), base::TimeTicks::Now()));
}

void CoralController::CacheEmbeddings(const CoralRequest& request) {
  CoralProcessor* coral_processor = EnsureCoralProcessor(request.language());
  if (!coral_processor) {
    LOG(ERROR) << "Failed to connect to coral processor.";
    return;
  }

  auto cache_embeddings_request = coral::mojom::CacheEmbeddingsRequest::New();
  cache_embeddings_request->embedding_options =
      coral::mojom::EmbeddingOptions::New();
  for (const auto& entity : request.content()) {
    cache_embeddings_request->entities.push_back(entity->Clone());
  }

  coral_processor->CacheEmbeddings(
      std::move(cache_embeddings_request),
      base::BindOnce(&CoralController::HandleCacheEmbeddingsResult,
                     weak_factory_.GetWeakPtr()));
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

void CoralController::CreateSavedDeskFromGroup(const std::string& template_name,
                                               coral::mojom::GroupPtr group,
                                               aura::Window* root_window) {
  std::vector<coral::mojom::EntityPtr> tab_app_entities =
      mojo::Clone(group->entities);
  base::flat_set<std::string> app_ids;
  for (const coral::mojom::EntityPtr& entity : tab_app_entities) {
    if (entity->is_app()) {
      app_ids.insert(entity->get_app()->id);
    }
  }

  // `RestoreDataCollector` has a callback because it is compatible with lacros
  // which needs to be async. If there are no apps and just tabs, we can
  // directly trigger the callback, but we have to create the template
  // ourselves.
  auto window_tracker = std::make_unique<aura::WindowTracker>(
      aura::WindowTracker::WindowList{root_window});
  if (app_ids.empty()) {
    OnTemplateCreated(std::move(tab_app_entities), std::move(window_tracker),
                      template_name,
                      /*desk_template=*/nullptr);
    return;
  }

  DesksController::Get()->CaptureActiveDeskAsSavedDesk(
      base::BindOnce(&CoralController::OnTemplateCreated,
                     weak_factory_.GetWeakPtr(), std::move(tab_app_entities),
                     std::move(window_tracker), template_name),
      DeskTemplateType::kCoral,
      /*root_window_to_show=*/root_window, app_ids);
}

void CoralController::OpenFeedbackDialog(const std::string& group_description) {
  Shell::Get()->coral_delegate()->OpenFeedbackDialog(
      group_description,
      base::BindOnce(&CoralController::OnFeedbackSendButtonClicked,
                     weak_factory_.GetWeakPtr()));
}

CoralController::CoralProcessor* CoralController::EnsureCoralProcessor(
    std::string language) {
  // Generate a fake processor if --force-birch-fake-coral-backend is enabled.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchFakeCoralBackend)) {
    if (!fake_processor_) {
      fake_processor_ = std::make_unique<FakeCoralProcessor>();
    }
    return fake_processor_.get();
  }

  if (coral_processor_) {
    return coral_processor_.get();
  }

  if (!coral_service_ && mojo_service_manager::IsServiceManagerBound()) {
    auto pipe_handle = coral_service_.BindNewPipeAndPassReceiver().PassPipe();
    mojo_service_manager::GetServiceManagerProxy()->Request(
        chromeos::mojo_services::kCrosCoralService, kRequestCoralServiceTimeout,
        std::move(pipe_handle));
    coral_service_.reset_on_disconnect();
  }

  if (coral_service_) {
    mojo::PendingRemote<
        chromeos::machine_learning::mojom::MachineLearningService>
        ml_service;
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(
            ml_service.InitWithNewPipeAndPassReceiver());
    coral_service_->Initialize(std::move(ml_service),
                               coral_processor_.BindNewPipeAndPassReceiver(),
                               language);
    coral_processor_.reset_on_disconnect();
  }

  return coral_processor_ ? coral_processor_.get() : nullptr;
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
    coral::mojom::CacheEmbeddingsResultPtr result) {
  if (result->is_error()) {
    LOG(ERROR) << "Coral cache embeddings request failed with CoralError code: "
               << static_cast<int>(result->get_error());
    return;
  }
}

void CoralController::OnTemplateCreated(
    std::vector<coral::mojom::EntityPtr> tab_app_entities,
    std::unique_ptr<aura::WindowTracker> window_tracker,
    const std::string& template_name,
    std::unique_ptr<DeskTemplate> desk_template) {
  std::unique_ptr<DeskTemplate> new_template = std::move(desk_template);
  std::vector<GURL> tab_urls;
  for (const auto& entity : tab_app_entities) {
    if (entity->is_tab()) {
      tab_urls.emplace_back(entity->get_tab()->url);
    }
  }

  // There is a chance the template is empty due to unsupported apps.
  if (!new_template) {
    if (tab_urls.empty()) {
      return;
    }

    new_template = std::make_unique<DeskTemplate>(
        base::Uuid::GenerateRandomV4(), DeskTemplateSource::kUser,
        template_name, base::Time::Now(), DeskTemplateType::kCoral);
    new_template->set_desk_restore_data(
        std::make_unique<app_restore::RestoreData>());
  } else {
    new_template->set_template_name(base::UTF8ToUTF16(template_name));
  }

  // To limit the memory usage, only save the first
  // `kMaxItemsForCoralSuppressionContext` items to build the suppression
  // context.
  const int entities_size = tab_app_entities.size();
  tab_app_entities.resize(
      std::min(entities_size, kMaxItemsForCoralSuppressionContext));
  new_template->set_coral_tab_app_entities(std::move(tab_app_entities));

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

  shell->saved_desk_delegate()->GetDeskModel()->AddOrUpdateEntry(
      std::move(new_template),
      base::BindOnce(&CoralController::ShowSavedDeskLibrary,
                     weak_factory_.GetWeakPtr(), std::move(window_tracker)));
}

void CoralController::ShowSavedDeskLibrary(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    desks_storage::DeskModel::AddOrUpdateEntryStatus status,
    std::unique_ptr<DeskTemplate> saved_desk) {
  if (status != desks_storage::DeskModel::AddOrUpdateEntryStatus::kOk) {
    return;
  }

  if (auto* overview_session = OverviewController::Get()->overview_session()) {
    aura::Window* root_window = window_tracker->windows().empty()
                                    ? Shell::GetPrimaryRootWindow()
                                    : window_tracker->windows()[0].get();
    overview_session->ShowSavedDeskLibrary(saved_desk->uuid(),
                                           /*saved_desk_name=*/u"",
                                           root_window);
  }
}

void CoralController::OnFeedbackSendButtonClicked(
    ScannerFeedbackInfo feedback_info,
    const std::string& user_description) {
  // Combine the group info from action details with the user description.
  std::string description =
      base::StrCat({"group items:  ", feedback_info.action_details,
                    "\nuser_description:  ", user_description, "\n"});
  Shell::Get()->shell_delegate()->SendSpecializedFeatureFeedback(
      Shell::Get()->session_controller()->GetActiveAccountId(),
      feedback::kCoralFeedbackProductId, std::move(description),
      /*image=*/std::nullopt, /*image_mime_type=*/std::nullopt);
}

}  // namespace ash
