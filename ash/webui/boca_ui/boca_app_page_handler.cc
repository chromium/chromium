// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/webui/boca_ui/boca_util.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-data-view.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/content_settings_handler.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/boca_metrics_util.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/boca_session_util.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/add_students_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/create_session_request.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/join_session_request.h"
#include "chromeos/ash/components/boca/session_api/remove_student_request.h"
#include "chromeos/ash/components/boca/session_api/renotify_student_request.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/session_api/update_session_request.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_constants.h"
#include "chromeos/ash/components/boca/student_screen_presenter.h"
#include "chromeos/ash/components/boca/teacher_screen_presenter.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/wm/constants.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash::boca {

namespace {
// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";

std::string GetReceiverName(std::string receiver_id,
                            PrefService* pref_service) {
  const auto& receiverCodes = pref_service->GetDict(
      ash::prefs::kClassManagementToolsKioskReceiverCodes);
  auto* receiver_name = receiverCodes.FindString(receiver_id);
  return receiver_name ? *receiver_name : "";
}

std::unique_ptr<::boca::OnTaskConfig> OnTaskConfigMojomToProto(
    const mojom::OnTaskConfigPtr& config) {
  auto on_task_config = std::make_unique<::boca::OnTaskConfig>();
  auto* active_bundle = on_task_config->mutable_active_bundle();
  active_bundle->set_locked(config->is_locked);
  active_bundle->set_lock_to_app_home(config->is_paused);

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
    const mojom::CaptionConfigPtr& config) {
  auto captions_config = std::make_unique<::boca::CaptionsConfig>();
  captions_config->set_captions_enabled(config->session_caption_enabled);
  captions_config->set_translations_enabled(
      config->session_translation_enabled);
  return captions_config;
}

mojom::ConfigPtr SessionConfigProtoToMojom(
    ::boca::Session* session,
    mojom::CaptionConfigPtr caption_config_override) {
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

  mojom::CaptionConfigPtr caption_config = mojom::CaptionConfig::New();
  if (caption_config_override) {
    caption_config = std::move(caption_config_override);
  } else if (GetSessionConfigSafe(session).has_captions_config()) {
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
        session_on_task_config.active_bundle().locked(),
        session_on_task_config.active_bundle().lock_to_app_home(),
        std::move(tabs));
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
  for (auto& item : activities) {
    auto student_status_detail =
        mojom::StudentStatusDetail(item.second.state());
    bool device_active = false;
    std::string active_tab;
    std::string connection_code;
    if (auto const device = item.second.devices().begin();
        device != item.second.devices().end()) {
      // Only update state and active tab for the first device now.
      // TODO - crbug.com/403655119: Ideally we should support multi-device. But
      // since now UI only supports single device, always parse the first one to
      // make the behavior deterministic.
      device_active = device->second.state() == ::boca::StudentDevice::ACTIVE;
      active_tab = device->second.activity().active_tab().title();
      connection_code = device->second.view_screen_config()
                            .connection_param()
                            .connection_code();
    }

    // If sign in more than one device, flip status to be inactive and
    if (item.second.devices().size() > 1) {
      device_active = false;
      student_status_detail =
          mojom::StudentStatusDetail::kMultipleDeviceSignedIn;
    }

    auto identity_ptr = mojom::IdentifiedActivity::New(
        item.first, mojom::StudentActivity::New(
                        student_status_detail, device_active, active_tab,
                        /*is_caption_enabled=*/false,
                        /*is_hand_raised=*/false, mojom::JoinMethod::kRoster,
                        connection_code));
    result.push_back(std::move(identity_ptr));
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
    case mojom::BocaValidPref::kOOBEAccessCount:
      return ash::prefs::kClassManagementToolsOOBEAccessCountSetting;
    case mojom::BocaValidPref::kKioskReceiverCodes:
      return ash::prefs::kClassManagementToolsKioskReceiverCodes;
  }
  NOTREACHED();
}

mojom::SpeechRecognitionInstallState GetMojomSodaState(
    BocaSessionManager::SodaStatus status) {
  switch (status) {
    case BocaSessionManager::SodaStatus::kUninstalled:
      return mojom::SpeechRecognitionInstallState::kUnknown;
    case BocaSessionManager::SodaStatus::kInstalling:
      return mojom::SpeechRecognitionInstallState::kInProgress;
    case BocaSessionManager::SodaStatus::kLanguageUnavailable:
      return mojom::SpeechRecognitionInstallState::kSystemLanguageUnsupported;
    case BocaSessionManager::SodaStatus::kInstallationFailure:
      return mojom::SpeechRecognitionInstallState::kFailed;
    case BocaSessionManager::SodaStatus::kReady:
      return mojom::SpeechRecognitionInstallState::kReady;
  }
  NOTREACHED();
}

mojom::CrdConnectionState GetMojomCrdConnectionState(CrdConnectionState state) {
  switch (state) {
    case CrdConnectionState::kUnknown:
      return mojom::CrdConnectionState::kUnknown;
    case CrdConnectionState::kConnecting:
      return mojom::CrdConnectionState::kConnecting;
    case CrdConnectionState::kConnected:
      return mojom::CrdConnectionState::kConnected;
    case CrdConnectionState::kDisconnected:
    case CrdConnectionState::kTimeout:
      return mojom::CrdConnectionState::kDisconnected;
    case CrdConnectionState::kFailed:
      return mojom::CrdConnectionState::kFailed;
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
      web_ui_(web_ui),
      session_manager_(BocaAppClient::Get()->GetSessionManager()) {
  auto* user = ash::BrowserContextHelper::Get()->GetUserByBrowserContext(
      web_ui->GetWebContents()->GetBrowserContext());
  user_identity_.set_email(user->GetAccountId().GetUserEmail());
  user_identity_.set_gaia_id(user->GetAccountId().GetGaiaId().ToString());
  user_identity_.set_full_name(base::UTF16ToUTF8(user->GetDisplayName()));
  SetAccountImage(user);
  pref_service_ = user->GetProfilePrefs();
  // BocaAppClient is guaranteed to be live here.
  GetSessionManager()->AddObserver(this);
  network_info_provider_ = std::make_unique<NetworkInfoProvider>(
      base::BindRepeating(&BocaAppHandler::OnActiveNetworkStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  base_url_ = BocaAppClient::Get()->GetSchoolToolsServerBaseUrl();
  ResetProducerSessionCaptionConfig();
}

BocaAppHandler::~BocaAppHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_producer_ &&
      producer_current_session_caption_config_->session_caption_enabled) {
    ::boca::CaptionsConfig caption_config;
    caption_config.set_captions_enabled(false);
    GetSessionManager()->NotifySessionCaptionProducerEvents(caption_config);
  }
  GetSessionManager()->RemoveObserver(this);
  if (!is_producer_ || (BocaAppClient::Get()->GetAppInstanceCount() > 1)) {
    // Always try end session when handler destructed, but do not proceed if
    // there are other app instances open. The total instance count will not be
    // decremented until all the app instance's tabs (including the one hosting
    // this) are closed and the Browser instance is scheduled for deletion.
    return;
  }
  GetSessionManager()->CleanupPresenters();
  // Best effort end session. Not handling response, if update failed,
  // persistent notification will stay.
  EndSession(base::BindOnce([](std::optional<mojom::UpdateSessionError>) {}));
  if (ash::features::IsAnnotatorModeEnabled() && is_producer_) {
    ash::boca::util::EnableOrDisableMarkerMode(/*enable=*/false);
  }
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
  if (config->caption_config) {
    NotifyLocalCaptionConfigUpdate(config->caption_config->Clone());
  }

  if (GetSessionManager()->disabled_on_non_managed_network()) {
    std::move(callback).Run(mojom::CreateSessionError::kNetworkRestriction);
    return;
  }
  std::unique_ptr<CreateSessionRequest> request =
      std::make_unique<CreateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          config->session_duration,
          // User will always start session as active state.
          ::boca::Session::SessionState::Session_SessionState_ACTIVE,
          base::BindOnce(&BocaAppHandler::OnCreateSessionResponse,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  auto roster = std::make_unique<::boca::Roster>();
  // Always create student group even if start session with no students.
  auto* student_groups = roster->mutable_student_groups()->Add();
  for (auto& item : config->students) {
    auto* student = student_groups->mutable_students()->Add();
    student->set_gaia_id(item->id);
    student->set_email(item->email);
    student->set_full_name(item->name);
    student->set_photo_url(item->photo_url.value_or(GURL()).spec());
  }
    request->set_roster(std::move(roster));
  if (config->caption_config) {
    request->set_captions_config(
        CaptionConfigMojomToProto(config->caption_config));
  }

  if (config->on_task_config) {
    request->set_on_task_config(
        OnTaskConfigMojomToProto(config->on_task_config));
  }

  session_client_impl_->CreateSession(std::move(request));
}

void BocaAppHandler::GetSession(GetSessionCallback callback) {
  if (GetSessionManager()->disabled_on_non_managed_network()) {
    std::move(callback).Run(
        mojom::SessionResult::NewError(mojom::GetSessionError::kEmpty));
    GetSessionManager()->UpdateCurrentSession(nullptr, /*dispatch_event=*/true);
    return;
  }
  auto get_session_request = std::make_unique<GetSessionRequest>(
      session_client_impl_->sender(), base_url_, is_producer_,
      GaiaId(user_identity_.gaia_id()),
      base::BindOnce(&BocaAppHandler::OnGetSession,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  get_session_request->set_device_id(BocaAppClient::Get()->GetDeviceId());
  // Can't skip because UI is expecting the callback to run as response.
  session_client_impl_->GetSession(std::move(get_session_request),
                                   /*can_skip_duplicate_request=*/false);
}

void BocaAppHandler::EndSession(EndSessionCallback callback) {
  if (GetSessionManager()->end_session_callback_for_testing()) {
    CHECK_IS_TEST();
    std::move(GetSessionManager()->end_session_callback_for_testing()).Run();
  }
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }

  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          session->session_id(),
          base::BindOnce(&BocaAppHandler::OnEndSessionResponse,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  request->set_session_state(
      std::make_unique<::boca::Session::SessionState>(::boca::Session::PAST));
  session_client_impl_->UpdateSession(std::move(request));
}

void BocaAppHandler::ExtendSessionDuration(
    base::TimeDelta extended_duration,
    ExtendSessionDurationCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE ||
      extended_duration.is_negative()) {
    receiver_.ReportBadMessage("Extend session with invalid input.");
    return;
  }
  if (!has_blocking_request_) {
    SendUpdateSessionRequestForExtendSession(
        session->session_id(), extended_duration, std::move(callback));
    return;
  }
  pending_update_requests_.push(
      base::BindOnce(&BocaAppHandler::SendUpdateSessionRequestForExtendSession,
                     base::Unretained(this), session->session_id(),
                     extended_duration, std::move(callback)));
}

void BocaAppHandler::RemoveStudent(const std::string& id,
                                   RemoveStudentCallback callback) {
  auto* session = GetSessionManager()->GetCurrentSession();
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

void BocaAppHandler::RenotifyStudent(const std::string& id,
                                     RenotifyStudentCallback callback) {
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(mojom::RenotifyStudentError::kInvalid);
    return;
  }

  std::unique_ptr<RenotifyStudentRequest> request =
      std::make_unique<RenotifyStudentRequest>(
          session_client_impl_->sender(), base_url_,
          GaiaId(user_identity_.gaia_id()), session->session_id(),
          base::BindOnce(&BocaAppHandler::OnRenotifiedStudent,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  request->set_student_ids({id});
  session_client_impl_->RenotifyStudent(std::move(request));
}

void BocaAppHandler::AddStudents(const std::vector<mojom::IdentityPtr> students,
                                 AddStudentsCallback callback) {
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    receiver_.ReportBadMessage("Extend session with invalid input.");
    return;
  }

  std::unique_ptr<AddStudentsRequest> request =
      std::make_unique<AddStudentsRequest>(
          session_client_impl_->sender(), base_url_,
          GaiaId(user_identity_.gaia_id()), session->session_id(),
          base::BindOnce(&BocaAppHandler::OnStudentsAdded,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         session));
  std::vector<::boca::UserIdentity> students_list;
  for (auto& item : students) {
    ::boca::UserIdentity student;
    student.set_gaia_id(item->id);
    student.set_email(item->email);
    student.set_full_name(item->name);
    student.set_photo_url(item->photo_url.value_or(GURL()).spec());
    students_list.push_back(student);
  }
  request->set_students(std::move(students_list));
  request->set_student_group_id(GetStudentGroupIdSafe(session));
  session_client_impl_->AddStudents(std::move(request));
}

void BocaAppHandler::UpdateOnTaskConfig(mojom::OnTaskConfigPtr config,
                                        UpdateOnTaskConfigCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE ||
      !config) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }
  if (!has_blocking_request_) {
    SendUpdateSessionRequestForOnTaskConfig(
        session->session_id(), std::move(config), std::move(callback));
    return;
  }
  pending_update_requests_.push(
      base::BindOnce(&BocaAppHandler::SendUpdateSessionRequestForOnTaskConfig,
                     base::Unretained(this), session->session_id(),
                     std::move(config), std::move(callback)));
}

void BocaAppHandler::UpdateCaptionConfig(mojom::CaptionConfigPtr config,
                                         UpdateCaptionConfigCallback callback) {
  // Dispatch local caption config.
  NotifyLocalCaptionConfigUpdate(config->Clone());

  // Dispatch remote caption config.
  auto* session = GetSessionManager()->GetCurrentSession();
  // Only producer can update session captions config and the session has to be
  // active.
  if (!session || session->session_state() != ::boca::Session::ACTIVE ||
      !is_producer_) {
    VLOG_IF(1, is_producer_)
        << "[Boca] session inactive, skipping captions update";
    std::move(callback).Run(std::nullopt);
    return;
  }
  // If no session config update, skip network request.
  if (producer_current_session_caption_config_->session_caption_enabled ==
          config->session_caption_enabled &&
      producer_current_session_caption_config_->session_translation_enabled ==
          config->session_translation_enabled) {
    VLOG(1) << "[Boca] no config change, skipping captions update. Captions "
               "enabled: "
            << config->session_caption_enabled
            << ", translation enabled: " << config->session_translation_enabled;
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Skip caption initialization when disabling captions.
  if (!config->session_caption_enabled) {
    VLOG(1) << "[Boca] captions disabled, skipping init session captions";
    UpdateCaptionConfigInternal(session->session_id(), std::move(config),
                                std::move(callback),
                                /*can_proceed=*/true);
    return;
  }
  GetSessionManager()->InitSessionCaption(
      base::BindOnce(&BocaAppHandler::UpdateCaptionConfigInternal,
                     weak_ptr_factory_.GetWeakPtr(), session->session_id(),
                     std::move(config), std::move(callback)));
}

void BocaAppHandler::SetFloatMode(bool is_float_mode,
                                  SetFloatModeCallback callback) {
  SetFloatModeAndBoundsForWindow(
      is_float_mode, web_ui_->GetWebContents()->GetTopLevelNativeWindow(),
      std::move(callback));
}

void BocaAppHandler::SubmitAccessCode(const std::string& access_code,
                                      SubmitAccessCodeCallback callback) {
  if (GetSessionManager()->disabled_on_non_managed_network()) {
    std::move(callback).Run(mojom::SubmitAccessCodeError::kNetworkRestriction);
    return;
  }
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
              boca::RecordViewStudentScreenErrorCode(result.error());
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
  if (student_screen_presenter() &&
      student_screen_presenter()->IsPresenting(id)) {
    // Already ended and a presentation is in progress.
    std::move(callback).Run(std::nullopt);
    return;
  }
  GetSessionManager()->EndSpotlightSession(base::DoNothing());
  EndViewScreenSessionInternal(id, std::move(callback));
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
              boca::RecordSetViewScreenSessionActiveErrorCode(result.error());
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
  GetSessionManager()->NotifyAppReload();
  std::move(callback).Run();
}

void BocaAppHandler::GetSpeechRecognitionInstallationStatus(
    GetSpeechRecognitionInstallationStatusCallback callback) {
  std::move(callback).Run(
      GetMojomSodaState(GetSessionManager()->GetSodaStatus()));
}

void BocaAppHandler::StartSpotlight(const std::string& crd_connection_code,
                                    StartSpotlightCallback callback) {
  if (!ash::features::IsBocaSpotlightRobotRequesterEnabled()) {
    std::move(callback).Run();
  }
  GetSessionManager()->StartCrdClient(
      crd_connection_code,
      base::BindOnce(&BocaAppHandler::OnCrdConnectionStateUpdated,
                     weak_ptr_factory_.GetWeakPtr(),
                     CrdConnectionState::kDisconnected),
      base::BindRepeating(&BocaAppHandler::OnCrdFrameReceived,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&BocaAppHandler::OnCrdConnectionStateUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
  std::move(callback).Run();
}

void BocaAppHandler::PresentStudentScreen(
    mojom::IdentityPtr student,
    const std::string& receiver_id,
    PresentStudentScreenCallback callback) {
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || !IsActiveSession(session->session_id())) {
    LOG(ERROR) << "[Boca] unexpected call to present student screen - no "
                  "active session";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kNoSession);
    std::move(callback).Run(false);
    return;
  }
  if (!student_screen_presenter()) {
    LOG(ERROR) << "[Boca] unexpected call to present student screen - no "
                  "student_screen_presenter";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kFeatureDisabled);
    std::move(callback).Run(false);
    return;
  }
  if (teacher_screen_presenter() &&
      teacher_screen_presenter()->IsPresenting()) {
    LOG(ERROR) << "[Boca] Trying to present student's screen while "
               << "presenting teacher's screen";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kTeacherScreenShareActive);
    std::move(callback).Run(false);
    return;
  }
  std::string student_id = student->id;
  auto end_view_screen_cb = base::BindOnce(
      &BocaAppHandler::OnEndViewScreenResponseForPresentStudentScreen,
      weak_ptr_factory_.GetWeakPtr(), session->session_id(), std::move(student),
      receiver_id, std::move(callback));
  auto end_spotlight_cb =
      base::BindOnce(&BocaAppHandler::EndViewScreenSessionInternal,
                     weak_ptr_factory_.GetWeakPtr(), std::move(student_id),
                     std::move(end_view_screen_cb));
  GetSessionManager()->EndSpotlightSession(std::move(end_spotlight_cb));
}

void BocaAppHandler::StopPresentingStudentScreen(
    StopPresentingStudentScreenCallback callback) {
  if (!student_screen_presenter()) {
    std::move(callback).Run(true);
    return;
  }
  student_screen_presenter()->Stop(std::move(callback));
}

void BocaAppHandler::PresentOwnScreen(const std::string& receiver_id,
                                      PresentOwnScreenCallback callback) {
  auto* session = GetSessionManager()->GetCurrentSession();
  bool is_session_active = session && IsActiveSession(session->session_id());
  if (!teacher_screen_presenter()) {
    LOG(ERROR) << "[Boca] unexpected call to present teacher's own screen";
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kFeatureDisabled, is_session_active);
    std::move(callback).Run(false);
    return;
  }
  if (student_screen_presenter() &&
      student_screen_presenter()->IsPresenting(/*student_id=*/std::nullopt)) {
    LOG(ERROR) << "[Boca] trying to present teacher's own screen while "
               << "presenting student's screen";
    RecordPresentOwnScreenResult(/* failure */ false, is_session_active);
    RecordPresentOwnScreenFailureReason(
        BocaPresentOwnScreenFailureReason::kStudentScreenShareActive,
        is_session_active);
    std::move(callback).Run(false);
    return;
  }
  teacher_screen_presenter()->Start(
      receiver_id, GetReceiverName(receiver_id, pref_service_), user_identity_,
      is_session_active, std::move(callback),
      base::BindOnce(&BocaAppHandler::OnPresentOwnScreenEnded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BocaAppHandler::StopPresentingOwnScreen(
    StopPresentingOwnScreenCallback callback) {
  if (!teacher_screen_presenter()) {
    std::move(callback).Run(true);
    return;
  }
  teacher_screen_presenter()->Stop(std::move(callback));
}

void BocaAppHandler::OnStudentActivityUpdated(
    std::vector<mojom::IdentifiedActivityPtr> activities) {
  remote_->OnStudentActivityUpdated(std::move(activities));
}

void BocaAppHandler::OnSessionConfigUpdated(mojom::ConfigResultPtr config) {
  remote_->OnSessionConfigUpdated(std::move(config));
}

void BocaAppHandler::OnCrdFrameReceived(
    SkBitmap bitmap,
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ash::features::IsBocaSpotlightRobotRequesterEnabled()) {
    return;
  }
  OnFrameDataReceived(std::move(bitmap));
}

void BocaAppHandler::OnCrdConnectionStateUpdated(CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ash::features::IsBocaSpotlightRobotRequesterEnabled()) {
    return;
  }
  OnSpotlightCrdSessionStatusUpdated(GetMojomCrdConnectionState(state));
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

void BocaAppHandler::OnSpeechRecognitionInstallStateUpdated(
    mojom::SpeechRecognitionInstallState) {}

void BocaAppHandler::OnSessionCaptionDisabled(bool is_error) {}

void BocaAppHandler::OnFrameDataReceived(const SkBitmap& frame_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_->OnFrameDataReceived(std::move(frame_data));
}

void BocaAppHandler::OnSpotlightCrdSessionStatusUpdated(
    mojom::CrdConnectionState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_->OnSpotlightCrdSessionStatusUpdated(std::move(state));
}

void BocaAppHandler::OnPresentStudentScreenEnded() {}

void BocaAppHandler::OnSessionStarted(const std::string& session_id,
                                      const ::boca::UserIdentity& producer) {
  ResetProducerSessionCaptionConfig();
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionMetadataUpdated(const std::string& session_id) {
  UpdateSessionConfig();
}

void BocaAppHandler::OnSessionEnded(const std::string& session_id) {
  ResetProducerSessionCaptionConfig();
  OnSessionConfigUpdated(
      mojom::ConfigResult::NewError(mojom::GetSessionError::kEmpty));
  if (student_screen_presenter() &&
      student_screen_presenter()->IsPresenting(/*student_id=*/std::nullopt)) {
    // Ending the session should disconnect the student remoting so update the
    // UI.
    remote_->OnPresentStudentScreenEnded();
    student_screen_presenter()->Stop(base::DoNothing());
  }
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_->OnLocalCaptionDisabled();
}

void BocaAppHandler::OnSodaStatusUpdate(BocaSessionManager::SodaStatus status) {
  remote_->OnSpeechRecognitionInstallStateUpdated(GetMojomSodaState(status));
}

void BocaAppHandler::OnSessionCaptionClosed(bool is_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_producer_) {
    LOG(ERROR) << "Session caption closed called on consumer.";
    return;
  }
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session || session->session_state() != ::boca::Session::ACTIVE) {
    return;
  }
  remote_->OnSessionCaptionDisabled(is_error);
  producer_current_session_caption_config_->session_caption_enabled = false;
  // Fire and forget captions update request, we don't need to handle the
  // response and the session captions will be disabled locally either way.
  UpdateCaptionConfigInternal(session->session_id(),
                              producer_current_session_caption_config_->Clone(),
                              /*callback=*/base::DoNothing(),
                              /*can_proceed=*/true);
}

void BocaAppHandler::OnReceiverInvalidation() {
  if (!student_screen_presenter()) {
    return;
  }
  student_screen_presenter()->CheckConnection();
}

void BocaAppHandler::OnPresentStudentScreenDisconnected() {
  remote_->OnPresentStudentScreenEnded();
}

void BocaAppHandler::NotifyLocalCaptionConfigUpdate(
    mojom::CaptionConfigPtr config) {
  ::boca::CaptionsConfig local_caption_config;
  local_caption_config.set_captions_enabled(config->local_caption_enabled);
  local_caption_config.set_translations_enabled(config->local_caption_enabled);
  GetSessionManager()->NotifyLocalCaptionEvents(
      std::move(local_caption_config));
}

void BocaAppHandler::SetSpotlightService(SpotlightService* spotlight_service) {
  spotlight_service_ = spotlight_service;
}

void BocaAppHandler::SetFloatModeAndBoundsForWindow(
    bool is_float_mode,
    aura::Window* window,
    SetFloatModeCallback callback) {
  if (!is_float_mode) {
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
  auto* session = GetSessionManager()->GetCurrentSession();
  if (!session) {
    return;
  }
  OnSessionConfigUpdated(
      mojom::ConfigResult::NewConfig(SessionConfigProtoToMojom(
          session, is_producer_
                       ? producer_current_session_caption_config_->Clone()
                       : nullptr)));
}

void BocaAppHandler::OnGetSession(
    GetSessionCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    boca::RecordGetSessionErrorCode(result.error());
    std::move(callback).Run(
        mojom::SessionResult::NewError(mojom::GetSessionError::kHTTPError));
    return;
  }
  if (!result.value() ||
      result.value()->session_state() != ::boca::Session::ACTIVE) {
    std::move(callback).Run(
        mojom::SessionResult::NewError(mojom::GetSessionError::kEmpty));
    // Load current session into memory;
    GetSessionManager()->UpdateCurrentSession(nullptr, /*dispatch_event=*/true);
    return;
  }
  auto session = std::move(result.value());
  auto student_activity =
      SessionActivityProtoToMojom(std::map<std::string, ::boca::StudentStatus>(
          session.get()->student_statuses().begin(),
          session.get()->student_statuses().end()));
  auto session_config = SessionConfigProtoToMojom(
      session.get(), is_producer_
                         ? producer_current_session_caption_config_->Clone()
                         : nullptr);
  std::move(callback).Run(mojom::SessionResult::NewSession(mojom::Session::New(
      std::move(session_config), std::move(student_activity))));

  // Load current session into memory;
  GetSessionManager()->UpdateCurrentSession(std::move(session),
                                            /*dispatch_event=*/true);
}

void BocaAppHandler::OnUpdatedSession(
    const std::string& session_id,
    UpdateSessionCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    boca::RecordUpdateSessionErrorCode(result.error());
    std::move(callback).Run(mojom::UpdateSessionError::kHTTPError);
  } else {
    std::move(callback).Run(std::nullopt);
    if (IsActiveSession(session_id)) {
      // Trigger a session reload from session response.
      GetSessionManager()->UpdateCurrentSession(std::move(result.value()),
                                                /*dispatch_event=*/true);
    }
  }
  OnUpdateSessionBlockingRequestCompleted();
}

void BocaAppHandler::OnUpdatedCaptionConfig(
    const std::string& session_id,
    UpdateCaptionConfigCallback callback,
    ::boca::CaptionsConfig captions_config,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    boca::RecordUpdateCaptionErrorCode(result.error());
    VLOG(1) << "[Boca] captions update session request failed with code "
            << result.error();
  }
  // We should not block producer from stopping sending session captions even
  // if the update fails. So handle failure only if the producer was trying to
  // enable captions and ignore it otherwise.
  if (!result.has_value() && captions_config.captions_enabled()) {
    std::move(callback).Run(mojom::UpdateSessionError::kHTTPError);
  } else {
    producer_current_session_caption_config_->session_caption_enabled =
        captions_config.captions_enabled();
    producer_current_session_caption_config_->session_translation_enabled =
        captions_config.translations_enabled();
    std::move(callback).Run(std::nullopt);
    VLOG(1) << "[Boca] captions update session result, captions enabled: "
            << GetSessionConfigSafe(result.value().get())
                   .captions_config()
                   .captions_enabled()
            << ", translation enabled: "
            << GetSessionConfigSafe(result.value().get())
                   .captions_config()
                   .translations_enabled();
    if (result.has_value()) {
      // Trigger a session reload from session response.
      GetSessionManager()->UpdateCurrentSession(std::move(result.value()),
                                                /*dispatch_event=*/true);
    }
    GetSessionManager()->NotifySessionCaptionProducerEvents(captions_config);
  }
  OnUpdateSessionBlockingRequestCompleted();
}

void BocaAppHandler::OnStudentRemoved(
    RemoveStudentCallback callback,
    ::boca::Session* current_session,
    std::string id,
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    boca::RecordRemoveStudentErrorCode(result.error());
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

void BocaAppHandler::OnRenotifiedStudent(
    RenotifyStudentCallback callback,
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    std::move(callback).Run(mojom::RenotifyStudentError::kHTTPError);
    return;
  }

  std::move(callback).Run(std::nullopt);
}

void BocaAppHandler::OnStudentsAdded(
    AddStudentsCallback callback,
    ::boca::Session* current_session,
    base::expected<bool, google_apis::ApiErrorCode> result) {
  if (!result.has_value()) {
    boca::RecordAddStudentsErrorCode(result.error());
    std::move(callback).Run(mojom::AddStudentsError::kHTTPError);
    return;
  }

  std::move(callback).Run(std::nullopt);
  GetSessionManager()->LoadCurrentSession(
      /*from_polling=*/false);
}

void BocaAppHandler::OnAccessCodeSubmitted(
    SubmitAccessCodeCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    boca::RecordJoinSessionViaAccessCodeErrorCode(result.error());
    std::move(callback).Run(mojom::SubmitAccessCodeError::kInvalid);
    return;
  } else {
    // Load current session into memory;
    GetSessionManager()->UpdateCurrentSession(std::move(result.value()),
                                              /*dispatch_event=*/true);
    std::move(callback).Run(std::nullopt);
  }
}

void BocaAppHandler::OnCreateSessionResponse(
    CreateSessionCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    boca::RecordCreateSessionErrorCode(result.error());
    std::move(callback).Run(mojom::CreateSessionError::kHTTPError);
    return;
  }
  // Load current session into memory;
  GetSessionManager()->UpdateCurrentSession(std::move(result.value()),
                                            /*dispatch_event=*/true);
  std::move(callback).Run(std::nullopt);
}

void BocaAppHandler::OnEndSessionResponse(
    EndSessionCallback callback,
    base::expected<std::unique_ptr<::boca::Session>, google_apis::ApiErrorCode>
        result) {
  if (!result.has_value()) {
    boca::RecordEndSessionErrorCode(result.error());
    std::move(callback).Run(mojom::UpdateSessionError::kHTTPError);
    return;
  }
  std::move(callback).Run(std::nullopt);
  GetSessionManager()->UpdateCurrentSession(std::move(result.value()), true);
}

void BocaAppHandler::UpdateCaptionConfigInternal(
    const std::string& session_id,
    mojom::CaptionConfigPtr config,
    UpdateCaptionConfigCallback callback,
    bool can_proceed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!can_proceed) {
    LOG(ERROR) << "[Boca] Caption initialization failed.";
    std::move(callback).Run(mojom::UpdateSessionError::kPreconditionFailed);
    return;
  }
  if (!has_blocking_request_) {
    SendUpdateSessionRequestForCaptionConfig(session_id, std::move(config),
                                             std::move(callback));
    return;
  }
  pending_update_requests_.push(
      base::BindOnce(&BocaAppHandler::SendUpdateSessionRequestForCaptionConfig,
                     base::Unretained(this), session_id, std::move(config),
                     std::move(callback)));
}

void BocaAppHandler::ResetProducerSessionCaptionConfig() {
  if (!is_producer_) {
    return;
  }
  producer_current_session_caption_config_ = mojom::CaptionConfig::New();
  producer_current_session_caption_config_->session_caption_enabled = false;
  producer_current_session_caption_config_->session_translation_enabled = false;
}

void BocaAppHandler::SendUpdateSessionRequestForExtendSession(
    const std::string& session_id,
    base::TimeDelta extended_duration,
    ExtendSessionDurationCallback callback) {
  if (!IsActiveSession(session_id)) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }
  auto* session = GetSessionManager()->GetCurrentSession();
  std::unique_ptr<UpdateSessionRequest> request =
      std::make_unique<UpdateSessionRequest>(
          session_client_impl_->sender(), base_url_, user_identity_,
          session->session_id(),
          base::BindOnce(&BocaAppHandler::OnUpdatedSession,
                         weak_ptr_factory_.GetWeakPtr(), session_id,
                         std::move(callback)));
  // TODO: crbug.com/391945140 - Remove redundant unique pointer dependencies.
  request->set_duration(std::make_unique<base::TimeDelta>(base::Seconds(
      session->duration().seconds() + extended_duration.InSeconds())));
  SendUpdateSessionRequestAndBlock(std::move(request));
}

void BocaAppHandler::SendUpdateSessionRequestForOnTaskConfig(
    const std::string& session_id,
    mojom::OnTaskConfigPtr config,
    UpdateOnTaskConfigCallback callback) {
  if (!IsActiveSession(session_id)) {
    std::move(callback).Run(mojom::UpdateSessionError::kInvalid);
    return;
  }
  auto* const session = GetSessionManager()->GetCurrentSession();
  auto request = std::make_unique<UpdateSessionRequest>(
      session_client_impl_->sender(), base_url_, user_identity_, session_id,
      base::BindOnce(&BocaAppHandler::OnUpdatedSession,
                     weak_ptr_factory_.GetWeakPtr(), session_id,
                     std::move(callback)));
  request->set_captions_config(std::make_unique<::boca::CaptionsConfig>(
      GetSessionConfigSafe(session).captions_config()));
  request->set_on_task_config(OnTaskConfigMojomToProto(config));
  SendUpdateSessionRequestAndBlock(std::move(request));
}

void BocaAppHandler::SendUpdateSessionRequestForCaptionConfig(
    const std::string& session_id,
    mojom::CaptionConfigPtr config,
    UpdateCaptionConfigCallback callback) {
  if (!IsActiveSession(session_id)) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::unique_ptr<::boca::CaptionsConfig> captions_config_proto =
      CaptionConfigMojomToProto(config);
  auto* const session = GetSessionManager()->GetCurrentSession();
  auto request = std::make_unique<UpdateSessionRequest>(
      session_client_impl_->sender(), base_url_, user_identity_, session_id,
      base::BindOnce(&BocaAppHandler::OnUpdatedCaptionConfig,
                     weak_ptr_factory_.GetWeakPtr(), session_id,
                     std::move(callback), *captions_config_proto));
  request->set_captions_config(std::move(captions_config_proto));
  request->set_on_task_config(std::make_unique<::boca::OnTaskConfig>(
      GetSessionConfigSafe(session).on_task_config()));
  SendUpdateSessionRequestAndBlock(std::move(request));
}

void BocaAppHandler::SendUpdateSessionRequestAndBlock(
    std::unique_ptr<UpdateSessionRequest> request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Block any new update session config requests until this request is
  // completed.
  has_blocking_request_ = true;
  session_client_impl_->UpdateSession(std::move(request));
}

bool BocaAppHandler::IsActiveSession(const std::string& session_id) {
  auto* session = GetSessionManager()->GetCurrentSession();
  return session && session->session_state() == ::boca::Session::ACTIVE &&
         session->session_id() == session_id;
}

void BocaAppHandler::OnUpdateSessionBlockingRequestCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  has_blocking_request_ = false;
  if (pending_update_requests_.empty()) {
    return;
  }
  base::OnceClosure update_request_cb =
      std::move(pending_update_requests_.front());
  pending_update_requests_.pop();
  std::move(update_request_cb).Run();
}

BocaSessionManager* BocaAppHandler::GetSessionManager() {
  return session_manager_;
}

void BocaAppHandler::SetAccountImage(user_manager::User* user) {
  auto* identity_manager = BocaAppClient::Get()->GetIdentityManager();
  if (!identity_manager) {
    return;
  }

  auto account_id = user->GetAccountId();
  if (account_id.GetAccountType() != AccountType::GOOGLE) {
    // Account type might not be GOOGLE during tests.
    return;
  }

  AccountInfo maybe_account_info =
      identity_manager->FindExtendedAccountInfoByGaiaId(account_id.GetGaiaId());
  if (!maybe_account_info.IsEmpty()) {
    user_identity_.set_photo_url(
        webui::GetBitmapDataUrl(maybe_account_info.account_image.AsBitmap()));
  }
}

void BocaAppHandler::OnPresentOwnScreenEnded() {
  remote_->OnPresentOwnScreenEnded();
}

void BocaAppHandler::EndViewScreenSessionInternal(
    const std::string& id,
    EndViewScreenSessionCallback callback) {
  CHECK(spotlight_service_);

  spotlight_service_->UpdateViewScreenState(
      id, ::boca::ViewScreenConfig::INACTIVE, base_url_,
      base::BindOnce(
          [](EndViewScreenSessionCallback cb,
             base::expected<bool, google_apis::ApiErrorCode> result) {
            if (!result.has_value()) {
              boca::RecordEndViewStudentScreenErrorCode(result.error());
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

void BocaAppHandler::PresentStudentScreenInternal(
    const std::string& session_id,
    mojom::IdentityPtr student,
    const std::string& receiver_id,
    PresentStudentScreenCallback callback) {
  if (!IsActiveSession(session_id)) {
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kNoSession);
    std::move(callback).Run(false);
    return;
  }
  if (!student_screen_presenter()) {
    LOG(ERROR) << "[Boca] unexpected call to present student screen";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kFeatureDisabled);
    std::move(callback).Run(false);
    return;
  }
  if (teacher_screen_presenter() &&
      teacher_screen_presenter()->IsPresenting()) {
    LOG(ERROR) << "[Boca] Trying to present student's screen while "
               << "presenting teacher's screen";
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kTeacherScreenShareActive);
    std::move(callback).Run(false);
    return;
  }
  ::boca::UserIdentity student_identity;
  student_identity.set_gaia_id(student->id);
  student_identity.set_email(student->email);
  student_identity.set_full_name(student->name);
  std::optional<std::string> student_device_id =
      GetSessionManager()->GetStudentActiveDeviceId(student->id);
  if (!student_device_id.has_value()) {
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kNoActiveStudentDevice);
    std::move(callback).Run(false);
    return;
  }
  student_screen_presenter()->Start(
      receiver_id, student_identity, student_device_id.value(),
      std::move(callback),
      base::BindOnce(
          &BocaSessionManager::NotifyPresentStudentScreenDisconnected,
          // Unretained is safe since `BocaSessionManager` owns
          // `StudentScreenPresenter`.
          base::Unretained(GetSessionManager())));
}

void BocaAppHandler::OnEndViewScreenResponseForPresentStudentScreen(
    const std::string& session_id,
    mojom::IdentityPtr student,
    const std::string& receiver_id,
    PresentStudentScreenCallback callback,
    std::optional<mojom::EndViewScreenSessionError> end_view_screen_error) {
  if (end_view_screen_error.has_value()) {
    RecordPresentStudentScreenResult(/* failure */ false);
    RecordPresentStudentScreenFailureReason(
        BocaPresentStudentScreenFailureReason::kEndSpotlightFailed);
    std::move(callback).Run(false);
    return;
  }
  PresentStudentScreenInternal(session_id, std::move(student), receiver_id,
                               std::move(callback));
}

TeacherScreenPresenter* BocaAppHandler::teacher_screen_presenter() {
  return GetSessionManager()->GetTeacherScreenPresenter();
}

StudentScreenPresenter* BocaAppHandler::student_screen_presenter() {
  return GetSessionManager()->GetStudentScreenPresenter();
}

}  // namespace ash::boca
