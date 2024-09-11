// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "content/public/browser/web_ui.h"

namespace ash::boca {

namespace {
// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";

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
  user_identity_ =
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId();
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
          session_client_impl_->sender(), user_identity_.GetGaiaId(),
          config->session_duration,
          // User will always start session as active state.
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          base::BindOnce(
              [](CreateSessionCallback callback,
                 base::expected<bool, google_apis::ApiErrorCode> result) {
                // TODO(b/358476060):Potentially parse error code to UI;
                if (!result.has_value()) {
                  std::move(callback).Run(false);
                } else {
                  std::move(callback).Run(true);
                }
              },
              std::move(callback)));
  if (!config->students.empty()) {
    std::vector<::boca::UserIdentity> identities;
    for (auto& item : config->students) {
      ::boca::UserIdentity student;
      student.set_gaia_id(item->id);
      student.set_email(item->email);
      student.set_full_name(item->name);
      // TODO(b/359045874): Set photo url.
      student.set_photo_url("");
      identities.push_back(std::move(student));
    }
    request->set_student_groups(std::move(identities));
  }
  if (config->caption_config) {
    auto captions_config = std::make_unique<::boca::CaptionsConfig>();
    captions_config->set_captions_enabled(
        config->caption_config->caption_enabled);
    captions_config->set_translations_enabled(
        config->caption_config->transcription_enabled);
    request->set_captions_config(std::move(captions_config));
  }

  if (config->on_task_config) {
    auto on_task_config = std::make_unique<::boca::OnTaskConfig>();
    auto* active_bundle = on_task_config->mutable_active_bundle();
    active_bundle->set_locked(config->on_task_config->is_locked);

    for (auto& item : config->on_task_config->tabs) {
      auto* content_config = active_bundle->mutable_content_configs()->Add();
      content_config->set_title(item->tab->title);
      content_config->set_url(item->tab->url.spec());
      content_config->set_favicon_url(item->tab->favicon);
      if (config->on_task_config->is_locked) {
        content_config->mutable_locked_navigation_options()
            ->set_navigation_type(
                ::boca::LockedNavigationOptions::NavigationType(
                    item->navigation_type));
      }
    }
    request->set_on_task_config(std::move(on_task_config));
  }

  session_client_impl_->CreateSession(std::move(request));
  NotifyLocalConfigUpdate(std::move(config));
}

void BocaAppHandler::GetSession(GetSessionCallback callback) {
  auto get_session_request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(), user_identity_.GetGaiaId(),
      base::BindOnce(
          [](GetSessionCallback callback,
             base::expected<std::unique_ptr<::boca::Session>,
                            google_apis::ApiErrorCode> result) {
            // TODO(b/358476060):Potentially parse error code to UI;
            if (!result.has_value()) {
              std::move(callback).Run(nullptr);
              return;
            }
            auto session = std::move(result.value());
            std::vector<mojom::IdentityPtr> students;
            for (auto student : GetStudentGroupsSafe(session.get())) {
              students.push_back(mojom::Identity::New(
                  student.gaia_id(), student.full_name(), student.email()));
            }

            auto caption_config = mojom::CaptionConfig::New();
            if (GetSessionConfigSafe(session.get()).has_captions_config()) {
              auto session_caption_config =
                  GetSessionConfigSafe(session.get()).captions_config();
              caption_config->caption_enabled =
                  session_caption_config.captions_enabled();
              caption_config->transcription_enabled =
                  session_caption_config.translations_enabled();
            }

            mojom::OnTaskConfigPtr on_task_config;
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

            auto config = mojom::Config::New(
                // Nanos are not used throughout session lifecycle so it's
                // safe to only parse seconds.
                base::Seconds(session->duration().seconds()),
                std::move(students), std::move(on_task_config),
                std::move(caption_config));

            std::move(callback).Run(std::move(config));
          },
          std::move(callback)));
  session_client_impl_->GetSession(std::move(get_session_request));
}

void BocaAppHandler::NotifyLocalConfigUpdate(mojom::ConfigPtr config) {
  if (auto caption_config = std::move(config->caption_config)) {
    ::boca::CaptionsConfig local_caption_config;
    // TODO(b/362291997):Update mojom to rename 'local_only' to be 'local'.
    local_caption_config.set_captions_enabled(caption_config->local_only);
    local_caption_config.set_translations_enabled(caption_config->local_only);
    BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionEvents(
        std::move(local_caption_config));
  }
}

}  // namespace ash::boca
