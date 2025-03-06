// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/content_settings_handler.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/join_session_request.h"
#include "chromeos/ash/components/boca/session_api/remove_student_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/wm/constants.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_id.h"

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
    content_config->set_favicon_url(item->tab->favicon.spec());
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

mojom::ConfigPtr SessionConfigProtoToMojom(::boca::Session* session) {
  CHECK(session);
  std::vector<mojom::IdentityPtr> students;
  std::vector<mojom::IdentityPtr> students_join_via_code;
  for (auto group : session->roster().student_groups()) {
    if (group.group_source() == ::boca::StudentGroup::CLASSROOM) {
      for (auto student : group.students()) {
        students.push_back(
            mojom::Identity::New(student.gaia_id(), student.full_name(),
                                 student.email(), GURL(student.photo_url())));
      }
    }
    if (group.group_source() == ::boca::StudentGroup::JOIN_CODE) {
      for (auto student : group.students()) {
        students_join_via_code.push_back(
            mojom::Identity::New(student.gaia_id(), student.full_name(),
                                 student.email(), GURL(student.photo_url())));
      }
    }
  }

  auto caption_config = mojom::CaptionConfig::New();
  if (GetSessionConfigSafe(session).has_captions_config()) {
    auto session_caption_config =
        GetSessionConfigSafe(session).captions_config();
    caption_config->session_caption_enabled =
        session_caption_config.captions_enabled();
    caption_config->session_translation_enabled =
        session_caption_config.translations_enabled();
  }

  mojom::OnTaskConfigPtr on_task_config = mojom::OnTaskConfig::New();
  if (GetSessionConfigSafe(session).has_on_task_config()) {
    auto session_on_task_config =
        GetSessionConfigSafe(session).on_task_config();
    std::vector<mojom::ControlledTabPtr> tabs;
    for (auto tab : session_on_task_config.active_bundle().content_configs()) {
      tabs.push_back(mojom::ControlledTab::New(
          mojom::TabInfo::New(std::nullopt, tab.title(), GURL(tab.url()),
                              GURL(tab.favicon_url())),
          mojom::NavigationType(
              tab.locked_navigation_options().navigation_type())));
    }
    on_task_config = mojom::OnTaskConfig::New(
        session_on_task_config.active_bundle().locked(), std::move(tabs));
  }
  mojom::IdentityPtr teacher;
  if (session->has_teacher()) {
    teacher = mojom::Identity::New(
        session->teacher().gaia_id(), session->teacher().full_name(),
        session->teacher().email(), GURL(session->teacher().photo_url()));
  }

  base::Time start_time;
  if (session->has_start_time()) {
    const auto nanos = session->start_time().nanos();
    const auto seconds = session->start_time().seconds();
    start_time = base::Time::FromSecondsSinceUnixEpoch(
        seconds +
        static_cast<double>(nanos) / base::Time::kNanosecondsPerSecond);
  }

  std::string access_code;
  if (session->has_join_code()) {
    access_code = session->join_code().code();
  }

  return mojom::Config::New(
      // Nanos are not used throughout session lifecycle so it's
      // safe to only parse seconds.
      base::Seconds(session->duration().seconds()), start_time,
      std::move(teacher), std::move(students),
      std::move(students_join_via_code), std::move(on_task_config),
      std::move(caption_config), access_code);
}

std::vector<mojom::IdentifiedActivityPtr> SessionActivityProtoToMojom(
    const std::map<std::string, ::boca::StudentStatus>& activities) {
  std::vector<mojom::IdentifiedActivityPtr> result;
  for (auto item : activities) {
    for (auto device : item.second.devices()) {
      // Only update state and active tab now.
      auto identity_ptr = mojom::IdentifiedActivity::New(
          item.first, mojom::StudentActivity::New(
                          item.second.state() == ::boca::StudentStatus::ACTIVE,
                          device.second.activity().active_tab().title(),
                          /*is_caption_enabled=*/false,
                          /*is_hand_raised=*/false, mojom::JoinMethod::kRoster,
                          device.second.view_screen_config()
                              .connection_param()
                              .connection_code()));
      result.push_back(std::move(identity_ptr));
    }
  }
  return result;
}

std::string GetPrefName(mojom::BocaValidPref pref) {
  switch (pref) {
    case mojom::BocaValidPref::kNavigationSetting:
      return ash::prefs::kClassManagementToolsNavRuleSetting;
    case mojom::BocaValidPref::kCaptionEnablementSetting:
      return ash::prefs::kClassManagementToolsCaptionEnablementSetting;
    case mojom::BocaValidPref::kDefaultMediaStreamSetting:
      return ::prefs::kManagedDefaultMediaStreamSetting;
  }
  NOTREACHED();
}

}  // namespace

BocaAppHandler::BocaAppHandler(
    mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
    mojo::PendingRemote<boca::mojom::Page> remote,
    content::WebUI* web_ui,
    std::unique_ptr<WebviewAuthHandler> auth_handler,
    std::unique_ptr<ClassroomPageHandlerImpl> classroom_client_impl,
    std::unique_ptr<ContentSettingsHandler> content_settings_handler,
    OnTaskSystemWebAppManager* system_web_app_manager,
    SessionClientImpl* session_client_impl,
    bool is_producer)
    : is_producer_(is_producer),
      tab_info_collector_(web_ui, is_producer),
      auth_handler_(std::move(auth_handler)),
      class_room_page_handler_(std::move(classroom_client_impl)),
      content_settings_handler_(std::move(content_settings_handler)),
      receiver_(this, std::move(receiver)),
      remote_(std::move(remote)),
      system_web_app_manager_(system_web_app_manager),
      session_client_impl_(session_client_impl),
      web_ui_(web_ui) {
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      web_ui->GetWebContents()->GetBrowserContext());
  user_identity_.set_email(user->GetAccountId().GetUserEmail());
  user_identity_.set_gaia_id(user->GetAccountId().GetGaiaId().ToString());
  user_identity_.set_full_name(base::UTF16ToUTF8(user->GetDisplayName()));
  user_identity_.set_photo_url(user->image_url().spec());
  pref_service_ = user->GetProfilePrefs();
  // BocaAppClient is guaranteed to be live here.
  BocaAppClient::Get()->GetSessionManager()->AddObserver(this);
  network_info_provider_ = std::make_unique<NetworkInfoProvider>(
      base::BindRepeating(&BocaAppHandler::OnActiveNetworkStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  BocaAppClient::Get()->GetSessionManager()->ToggleAppStatus(
      /*is_app_opened=*/true);
  base_url_ = BocaAppClient::Get()->GetSchoolToolsServerBaseUrl();
}

BocaAppHandler::~BocaAppHandler() {
  BocaAppClient::Get()->GetSessionManager()->RemoveObserver(this);
  BocaAppClient::Get()->GetSessionManager()->ToggleAppStatus(
      /*is_app_opened=*/false);
}

void BocaAppHandler::AuthenticateWebview(AuthenticateWebviewCallback callback) {
  auth_handler_->AuthenticateWebview(std::move(callback));
}

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

void BocaAppHandler::ListAssignments(const std::string& course_id,
                                     ListAssignmentsCallback callback) {
  class_room_page_handler_->ListAssignments(course_id, std::move(callback));
}

void BocaAppHandler::CreateSession(mojom::ConfigPtr config,
                                   CreateSessionCallback callback) {
  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          config->session_duration,
          // User will always start session as active state.
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          base::BindOnce(
              [](CreateSessionCallback callback,
                 base::expected<std::unique_ptr<::boca::Session>,
                                google_apis::ApiErrorCode> result) {
                // TODO(crbug.com/358476060): Potentially parse error code to
                // UI;
                if (!result.has_value()) {
                  std::move(callback).Run(false);
                } else {
                  // Load current session into memory;
                  BocaAppClient::Get()
                      ->GetSessionManager()
                      ->UpdateCurrentSession(std::move(result.value()),
                                             /*dispatch_event=*/true);
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
      session_client_impl_->sender(), base_url_, is_producer_,
      GaiaId(user_identity_.gaia_id()),
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
                  nullptr, /*dispatch_event=*/true);
              return;
            }
            auto session = std::move(result.value());
            auto student_activity = SessionActivityProtoToMojom(
                std::map<std::string, ::boca::StudentStatus>(
                    session.get()->student_statuses().begin(),
                    session.get()->student_statuses().end()));
            auto session_config = SessionConfigProtoToMojom(session.get());
            std::move(callback).Run(
                mojom::SessionResult::NewSession(mojom::Session::New(
                    std::move(session_config), std::move(student_activity))));

            // Load current session into memory;
            BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
                std::move(session), /*dispatch_event=*/true);
          },
          std::move(callback)));
  get_session_request->set_device_id(BocaAppClient::Get()->GetDeviceId());
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
          session_client_impl_->sender(), base_url_, user_identity_,
          session->session_id(),
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

void BocaAppHandler::ExtendSessionDuration(
    base::TimeDelta extended_duration,
    ExtendSessionDurationCallback callback) {
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE ||
      extended_duration.is_negative()) {
    receiver_.ReportBadMessage("Extend session with invalid input.");
    return;
  }
  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          session->session_id(),
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
  // TODO: crbug.com/391945140 - Remove redundant unique pointer dependencies.
  request->set_duration(std::make_unique<base::TimeDelta>(base::Seconds(
      session->duration().seconds() + extended_duration.InSeconds())));
  session_client_impl_->UpdateSession(std::move(request));
}

void BocaAppHandler::RemoveStudent(const std::string& id,
                                   RemoveStudentCallback callback) {
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(mojom::RemoveStudentError::kInvalid);
    return;
  }

  std::unique_ptr<RemoveStudentRequest> request =
      std::make_unique<RemoveStudentRequest>(
          session_client_impl_->sender(), base_url_,
          GaiaId(user_identity_.gaia_id()), session->session_id(),
          base::BindOnce(&BocaAppHandler::OnStudentRemoved,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         session, id));

  request->set_student_ids({id});
  session_client_impl_->RemoveStudent(std::move(request));
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
      session_client_impl_->sender(), base_url_, user_identity_,
      session->session_id(),
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
    std::move(callback).Run(std::nullopt);
    return;
  }

  // If no session config update, skip network request.
  if (GetSessionConfigSafe(session).captions_config().captions_enabled() ==
          config->session_caption_enabled &&
      GetSessionConfigSafe(session).captions_config().translations_enabled() ==
          config->session_translation_enabled) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          session->session_id(),
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

void BocaAppHandler::SetFloatMode(bool is_float_mode,
                                  SetFloatModeCallback callback) {
  SetFloatModeAndBoundsForWindow(
      is_float_mode, web_ui_->GetWebContents()->GetTopLevelNativeWindow(),
      std::move(callback));
}

void BocaAppHandler::SubmitAccessCode(const std::string& access_code,
                                      SubmitAccessCodeCallback callback) {
  std::unique_ptr<JoinSessionRequest> request =
      std::make_unique<JoinSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          BocaAppClient::Get()->GetDeviceId(), access_code,
          base::BindOnce(&BocaAppHandler::OnAccessCodeSubmitted,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  session_client_impl_->JoinSession(std::move(request));
}

void BocaAppHandler::ViewStudentScreen(const std::string& id,
                                       ViewStudentScreenCallback callback) {
  CHECK(spotlight_service_);
  spotlight_service_->ViewScreen(
      id, base_url_,
      base::BindOnce(
          [](ViewStudentScreenCallback callback,
             base::expected<bool, google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              LOG(WARNING) << "[Boca] Error requesting to view student screen: "
                           << result.error();
              std::move(callback).Run(
                  mojom::ViewStudentScreenError::kHTTPError);
              return;
            } else {
              std::move(callback).Run(std::nullopt);
            }
          },
          std::move(callback)));
}

void BocaAppHandler::EndViewScreenSession(
    const std::string& id,
    EndViewScreenSessionCallback callback) {
  CHECK(spotlight_service_);

  spotlight_service_->UpdateViewScreenState(
      id, ::boca::ViewScreenConfig::INACTIVE, base_url_,
      base::BindOnce(
          [](EndViewScreenSessionCallback cb,
             base::expected<bool, google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              LOG(WARNING)
                  << "[Boca] Error setting view screen state to inactive: "
                  << result.error();
              std::move(cb).Run(mojom::EndViewScreenSessionError::kHTTPError);
              return;
            }
            std::move(cb).Run(std::nullopt);
          },
          std::move(callback)));
}

void BocaAppHandler::SetViewScreenSessionActive(
    const std::string& id,
    SetViewScreenSessionActiveCallback callback) {
  CHECK(spotlight_service_);

  spotlight_service_->UpdateViewScreenState(
      id, ::boca::ViewScreenConfig::ACTIVE, base_url_,
      base::BindOnce(
          [](SetViewScreenSessionActiveCallback cb,
             base::expected<bool, google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              LOG(WARNING)
                  << "[Boca] Error setting view screen state to active: "
                  << result.error();
              std::move(cb).Run(
                  mojom::SetViewScreenSessionActiveError::kHTTPError);
              return;
            }
            std::move(cb).Run(std::nullopt);
          },
          std::move(callback)));
}

void BocaAppHandler::GetUserPref(mojom::BocaValidPref pref,
                                 GetUserPrefCallback callback) {
  const auto& value = pref_service_->GetValue(GetPrefName(pref));
  std::move(callback).Run(value.Clone());
}

void BocaAppHandler::SetUserPref(mojom::BocaValidPref pref,
                                 base::Value value,
                                 SetUserPrefCallback callback) {
  // Boca should only get but not set kDefaultMediaStreamSetting.
  if (pref == mojom::BocaValidPref::kDefaultMediaStreamSetting) {
    mojo::ReportBadMessage(
        "Attempted to set kDefaultMediaStreamSetting user pref.");
    return;
  }

  pref_service_->Set(GetPrefName(pref), std::move(value));
  std::move(callback).Run();
}

void BocaAppHandler::SetSitePermission(const std::string& url,
                                       mojom::Permission permission,
                                       mojom::PermissionSetting setting,
                                       SetSitePermissionCallback callback) {
  const bool success = content_settings_handler_->SetContentSettingForOrigin(
      url, permission, setting);
  std::move(callback).Run(success);
}

void BocaAppHandler::CloseTab(const SessionID::id_type tab_id,
                              CloseTabCallback callback) {
  if (!system_web_app_manager_) {
    std::move(callback).Run(false);
    return;
  }

  const SessionID window_id =
      system_web_app_manager_->GetActiveSystemWebAppWindowID();
  const SessionID id = SessionID::FromSerializedValue(tab_id);
  if (!window_id.is_valid() || !id.is_valid()) {
    std::move(callback).Run(false);
    return;
  }

  system_web_app_manager_->RemoveTabsWithTabIds(window_id, {id});
  std::move(callback).Run(true);
}

void BocaAppHandler::OpenFeedbackDialog(OpenFeedbackDialogCallback callback) {
  BocaAppClient::Get()->OpenFeedbackDialog();
  std::move(callback).Run();
}

void BocaAppHandler::RefreshWorkbook(RefreshWorkbookCallback callback) {
  BocaAppClient::Get()->GetSessionManager()->NotifyAppReload();
  std::move(callback).Run();
}

void BocaAppHandler::OnStudentActivityUpdated(
    std::vector<mojom::IdentifiedActivityPtr> activities) {
  remote_->OnStudentActivityUpdated(std::move(activities));
}

void BocaAppHandler::OnSessionConfigUpdated(mojom::ConfigResultPtr config) {
  remote_->OnSessionConfigUpdated(std::move(config));
}

void BocaAppHandler::OnActiveNetworkStateChanged(
    std::vector<mojom::NetworkInfoPtr> active_networks) {
  remote_->OnActiveNetworkStateChanged(std::move(active_networks));
}

void BocaAppHandler::OnConsumerActivityUpdated(
    const std::map<std::string, ::boca::StudentStatus>& activities) {
  OnStudentActivityUpdated(SessionActivityProtoToMojom(activities));
}

void BocaAppHandler::OnLocalCaptionDisabled() {}

void BocaAppHandler::OnSessionStarted(const std::string& session_id,
                                      const ::boca::UserIdentity& producer) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionMetadataUpdated(const std::string& session_id) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionEnded(const std::string& session_id) {
  OnSessionConfigUpdated(
      mojom::ConfigResult::NewError(mojom::GetSessionError::kEmpty));
}

void BocaAppHandler::OnBundleUpdated(const ::boca::Bundle& bundle) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config,
    const std::string& tachyon_group_id) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionRosterUpdated(const ::boca::Roster& roster) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnLocalCaptionClosed() {
  remote_->OnLocalCaptionDisabled();
}

void BocaAppHandler::NotifyLocalCaptionConfigUpdate(
    mojom::CaptionConfigPtr config) {
  ::boca::CaptionsConfig local_caption_config;
  local_caption_config.set_captions_enabled(config->local_caption_enabled);
  local_caption_config.set_translations_enabled(config->local_caption_enabled);
  BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionEvents(
      std::move(local_caption_config));
}

void BocaAppHandler::SetSpotlightService(SpotlightService* spotlight_service) {
  spotlight_service_ = spotlight_service;
}

void BocaAppHandler::SetFloatModeAndBoundsForWindow(
    bool isFloatMode,
    aura::Window* window,
    SetFloatModeCallback callback) {
  if (!isFloatMode) {
    // We don't unset float mode, do nothing here.
    std::move(callback).Run(false);
    return;
  }
  auto* window_state = ash::WindowState::Get(window);
  const ash::WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  // Have to explicitly set bound. Default to no animation.
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window);
  const int padding_dp = chromeos::wm::kFloatedWindowPaddingDp;
  const ash::SetBoundsWMEvent set_bound_event(
      gfx::Rect(gfx::Point(work_area.right() - padding_dp - 400,
                           work_area.y() + padding_dp),
                gfx::Size(400, 600)));
  window_state->OnWMEvent(&float_event);
  window_state->OnWMEvent(&set_bound_event);
  std::move(callback).Run(true);
}

void BocaAppHandler::UpdateSessionConfig() {
  auto* session =
      BocaAppClient::Get()->GetSessionManager()->GetCurrentSession();
  if (!session) {
    return;
  }
  OnSessionConfigUpdated(
      mojom::ConfigResult::NewConfig(SessionConfigProtoToMojom(session)));
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
      std::move(result.value()), /*dispatch_event=*/true);
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
      std::move(result.value()), /*dispatch_event=*/true);
}

void BocaAppHandler::OnStudentRemoved(
    RemoveStudentCallback callback,
    ::boca::Session* current_session,
    std::string id,
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    std::move(callback).Run(mojom::RemoveStudentError::kHTTPError);
    return;
  }

  std::move(callback).Run(std::nullopt);
  // Remove student from local session
  for (int i = 0; i < current_session->roster().student_groups().size(); i++) {
    auto* group = current_session->mutable_roster()->mutable_student_groups(i);
    for (int j = 0; j < group->students().size(); j++) {
      if (group->students()[j].gaia_id() == id) {
        group->mutable_students()->DeleteSubrange(j, 1);
        break;
      }
    }
  }
}

void BocaAppHandler::OnAccessCodeSubmitted(
    SubmitAccessCodeCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    std::move(callback).Run(mojom::SubmitAccessCodeError::kInvalid);
    return;
  } else {
    // Load current session into memory;
    BocaAppClient::Get()->GetSessionManager()->UpdateCurrentSession(
        std::move(result.value()), /*dispatch_event=*/true);
    std::move(callback).Run(std::nullopt);
  }
}
}  // namespace ash::boca
