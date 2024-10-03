// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "content/public/browser/web_ui.h"

namespace ash::boca {

namespace {
// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";

std::unique_ptr<::boca::OnTaskConfig> OnTaskConfigMojomToProto(
    mojom::OnTaskConfigPtr config) {
  auto on_task_config = std::make_unique<::boca::OnTaskConfig>();
  auto* active_bundle = on_task_config->mutable_active_bundle();
  active_bundle->set_locked(config->is_locked);

  for (auto& item : config->tabs) {
    auto* content_config = active_bundle->mutable_content_configs()->Add();
    content_config->set_title(item->tab->title);
    content_config->set_url(item->tab->url.spec());
    content_config->set_favicon_url(item->tab->favicon);
    content_config->mutable_locked_navigation_options()->set_navigation_type(
        ::boca::LockedNavigationOptions::NavigationType(item->navigation_type));
  }
  return on_task_config;
}

std::unique_ptr<::boca::CaptionsConfig> CaptionConfigMojomToProto(
    mojom::CaptionConfigPtr config) {
  auto captions_config = std::make_unique<::boca::CaptionsConfig>();
  captions_config->set_captions_enabled(config->session_caption_enabled);
  captions_config->set_translations_enabled(
      config->session_translation_enabled);
  return captions_config;
}
}  // namespace

BocaAppHandler::BocaAppHandler(
    BocaUI* boca_ui,
    mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
    mojo::PendingRemote<boca::mojom::Page> remote,
    content::WebUI* web_ui,
    std::unique_ptr<ClassroomPageHandlerImpl> classroom_client_impl,
    std::unique_ptr<SessionClientImpl> session_client_impl)
    : tab_info_collector_(web_ui),
      class_room_page_handler_(std::move(classroom_client_impl)),
      session_client_impl_(std::move(session_client_impl)),
      receiver_(this, std::move(receiver)),
      remote_(std::move(remote)),
      boca_ui_(boca_ui) {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  user_identity_.set_email(user->GetAccountId().GetUserEmail());
  user_identity_.set_gaia_id(user->GetAccountId().GetGaiaId());
  user_identity_.set_full_name(base::UTF16ToUTF8(user->GetDisplayName()));
}

BocaAppHandler::~BocaAppHandler() = default;

void BocaAppHandler::GetWindowsTabsList(GetWindowsTabsListCallback callback) {
  tab_info_collector_.GetWindowTabInfo(std::move(callback));
}

void BocaAppHandler::ListCourses(ListCoursesCallback callback) {
  class_room_page_handler_->ListCourses(kOwnCoursesFilterValue,
                                        std::move(callback));
}

void BocaAppHandler::ListStudents(const std::string& course_id,
                                  ListStudentsCallback callback) {
  class_room_page_handler_->ListStudents(course_id, std::move(callback));
}

void BocaAppHandler::CreateSession(mojom::ConfigPtr config,
                                   CreateSessionCallback callback) {
  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          session_client_impl_->sender(), user_identity_,
          config->session_duration,
          // User will always start session as active state.
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          base::BindOnce(
              [](CreateSessionCallback callback,
                 base::expected<std::unique_ptr<::boca::Session>,
                                google_apis::ApiErrorCode> result) {
                // TODO(b/358476060):Potentially parse error code to UI;
                if (!result.has_value()) {
                  std::move(callback).Run(false);
                } else {
                  // Load current session into memory;
                  BocaAppClient::Get()
                      ->GetSessionManager()
                      ->UpdateCurrentSession(std::move(result.value()), true);
                  std::move(callback).Run(true);
                }
              },
              std::move(callback)));
  if (!config->students.empty()) {
    auto roster = std::make_unique<::boca::Roster>();
    auto* student_groups = roster->mutable_student_groups()->Add();
    std::vector<::boca::UserIdentity> identities;
    for (auto& item : config->students) {
      auto* student = student_groups->mutable_students()->Add();
      student->set_gaia_id(item->id);
      student->set_email(item->email);
      student->set_full_name(item->name);
      student->set_photo_url(item->photo_url.value_or(GURL()).spec());
    }
    request->set_roster(std::move(roster));
  }
  if (config->caption_config) {
    request->set_captions_config(
        CaptionConfigMojomToProto(config->caption_config->Clone()));
  }

  if (config->on_task_config) {
    request->set_on_task_config(
        OnTaskConfigMojomToProto(config->on_task_config->Clone()));
  }

  session_client_impl_->CreateSession(std::move(request));

  if (auto caption_config = std::move(config->caption_config)) {
    NotifyLocalCaptionConfigUpdate(std::move(caption_config));
  }
}

void BocaAppHandler::GetSession(GetSessionCallback callback) {
  auto get_session_request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(), user_identity_.gaia_id(),
      base::BindOnce(
          [](GetSessionCallback callback,
             base::expected<std::unique_ptr<::boca::Session>,
                            google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              std::move(callback).Run(mojom::SessionResult::NewError(
                  mojom::GetSessionError::kHTTPError));
              return;
            }
            if (!result.value() ||
                result.value()->session_state() != ::boca::Session::ACTIVE) {
              std::move(callback).Run(mojom::SessionResult::NewError(
                  mojom::GetSessionError::kEmpty));
              // Load current session into memory;
              BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
                  nullptr, false);
              return;
            }
            auto session = std::move(result.value());
            std::vector<mojom::IdentityPtr> students;
            for (auto student : GetStudentGroupsSafe(session.get())) {
              students.push_back(mojom::Identity::New(
                  student.gaia_id(), student.full_name(), student.email(),
                  GURL(student.photo_url())));
            }

            auto caption_config = mojom::CaptionConfig::New();
            if (GetSessionConfigSafe(session.get()).has_captions_config()) {
              auto session_caption_config =
                  GetSessionConfigSafe(session.get()).captions_config();
              caption_config->session_caption_enabled =
                  session_caption_config.captions_enabled();
              caption_config->session_translation_enabled =
                  session_caption_config.translations_enabled();
            }

            mojom::OnTaskConfigPtr on_task_config = mojom::OnTaskConfig::New();
            if (GetSessionConfigSafe(session.get()).has_on_task_config()) {
              auto session_on_task_config =
                  GetSessionConfigSafe(session.get()).on_task_config();
              std::vector<mojom::ControlledTabPtr> tabs;
              for (auto tab :
                   session_on_task_config.active_bundle().content_configs()) {
                tabs.push_back(mojom::ControlledTab::New(
                    mojom::TabInfo::New(tab.title(), GURL(tab.url()),
                                        tab.favicon_url()),
                    mojom::NavigationType(
                        tab.locked_navigation_options().navigation_type())));
              }
              on_task_config = mojom::OnTaskConfig::New(
                  session_on_task_config.active_bundle().locked(),
                  std::move(tabs));
            }
            mojom::IdentityPtr teacher;
            if (session->has_teacher()) {
              teacher = mojom::Identity::New(
                  session->teacher().gaia_id(), session->teacher().full_name(),
                  session->teacher().email(),
                  GURL(session->teacher().photo_url()));
            }

            base::Time start_time;
            if (session->has_start_time()) {
              const auto nanos = session->start_time().nanos();
              const auto seconds = session->start_time().seconds();
              start_time = base::Time::FromSecondsSinceUnixEpoch(
                  seconds + static_cast<double>(nanos) /
                                base::Time::kNanosecondsPerSecond);
            }
            auto config = mojom::Config::New(
                // Nanos are not used throughout session lifecycle so it's
                // safe to only parse seconds.
                base::Seconds(session->duration().seconds()), start_time,
                std::move(teacher), std::move(students),
                std::move(on_task_config), std::move(caption_config));

            std::move(callback).Run(
                mojom::SessionResult::NewConfig(std::move(config)));

            // Load current session into memory;
            BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
                std::move(session), false);
          },
          std::move(callback)));
  session_client_impl_->GetSession(std::move(get_session_request));
}

void BocaAppHandler::EndSession(EndSessionCallback callback) {
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }
  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), user_identity_, session->session_id(),
          base::BindOnce(
              [](EndSessionCallback callback,
                 base::expected<std::unique_ptr<::boca::Session>,
                                google_apis::ApiErrorCode> result) {
                if (!result.has_value()) {
                  std::move(callback).Run(
                      mojom::UpdateSessionError::kHTTPError);
                  return;
                }
                std::move(callback).Run(std::nullopt);
                BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
                    std::move(result.value()), true);
              },
              std::move(callback)));
  request->set_session_state(
      std::make_unique<::boca::Session::SessionState>(::boca::Session::PAST));
  session_client_impl_->UpdateSession(std::move(request));
}

void BocaAppHandler::UpdateOnTaskConfig(mojom::OnTaskConfigPtr config,
                                        UpdateOnTaskConfigCallback callback) {
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE ||
      !config) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }

  auto request = std::make_unique<UpdateSessionRequest>(
      session_client_impl_->sender(), user_identity_, session->session_id(),
      base::BindOnce(&BocaAppHandler::OnUpdatedOnTaskConfig,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  auto on_task_config = OnTaskConfigMojomToProto(config->Clone());
  // Record the current on task update so that caption change won't override it.
  // Will be reset on callback run.
  latest_ontask_config_ =
      std::make_unique<::boca::OnTaskConfig>(*on_task_config);
  request->set_on_task_config(std::move(on_task_config));

  if (!latest_caption_config_) {
    latest_caption_config_ = std::make_unique<::boca::CaptionsConfig>(
        GetSessionConfigSafe(session).captions_config());
  }
  request->set_captions_config(std::move(latest_caption_config_));
  session_client_impl_->UpdateSession(std::move(request));
}

void BocaAppHandler::UpdateCaptionConfig(mojom::CaptionConfigPtr config,
                                         UpdateCaptionConfigCallback callback) {
  // Dispatch local caption config.
  NotifyLocalCaptionConfigUpdate(config->Clone());

  // Dispatch remote caption config.
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }
  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), user_identity_, session->session_id(),
          base::BindOnce(&BocaAppHandler::OnUpdatedCaptionConfig,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  auto captions_config = CaptionConfigMojomToProto(config->Clone());
  // Record the current caption update so that on task change won't override it.
  // Will be reset on callback run.
  latest_caption_config_ =
      std::make_unique<::boca::CaptionsConfig>(*captions_config);
  request->set_captions_config(std::move(captions_config));

  if (!latest_ontask_config_) {
    latest_ontask_config_ = std::make_unique<::boca::OnTaskConfig>(
        GetSessionConfigSafe(session).on_task_config());
  }
  request->set_on_task_config(std::move(latest_ontask_config_));
  session_client_impl_->UpdateSession(std::move(request));
}

void BocaAppHandler::NotifyLocalCaptionConfigUpdate(
    mojom::CaptionConfigPtr config) {
  ::boca::CaptionsConfig local_caption_config;
  local_caption_config.set_captions_enabled(config->local_caption_enabled);
  local_caption_config.set_translations_enabled(config->local_caption_enabled);
  BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionEvents(
      std::move(local_caption_config));
}

void BocaAppHandler::OnUpdatedOnTaskConfig(
    UpdateOnTaskConfigCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    std::move(callback).Run(mojom::UpdateSessionError::kHTTPError);
    // Update failed. Fallback to the most recent in-memory session.
    if (auto* session =
            BocaAppClient::Get()->GetSessionManager()->GetCurrentSession()) {
      latest_ontask_config_ = std::make_unique<::boca::OnTaskConfig>(
          GetSessionConfigSafe(session).on_task_config());
    } else {
      latest_ontask_config_.reset();
    }
    return;
  }
  std::move(callback).Run(std::nullopt);
  // Trigger a session reload from session response.
  BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
      std::move(result.value()), true);
}

void BocaAppHandler::OnUpdatedCaptionConfig(
    UpdateCaptionConfigCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    std::move(callback).Run(mojom::UpdateSessionError::kHTTPError);
    // Update failed. Fallback to the most recent in-memory session.
    if (auto* session =
            BocaAppClient::Get()->GetSessionManager()->GetCurrentSession()) {
      latest_caption_config_ = std::make_unique<::boca::CaptionsConfig>(
          GetSessionConfigSafe(session).captions_config());
    } else {
      latest_caption_config_.reset();
    }
    return;
  }
  std::move(callback).Run(std::nullopt);
  // Trigger a session reload from session response.
  BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
      std::move(result.value()), true);
}
}  // namespace ash::boca
