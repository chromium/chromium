// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom-forward.h"
#include "ash/webui/boca_ui/mojom/boca.mojom-shared.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/content_settings_handler.h"
#include "ash/webui/boca_ui/provider/network_info_provider.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "ash/webui/boca_ui/webview_auth_handler.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {

class BocaAppHandler : public mojom::PageHandler,
                       public mojom::Page,
                       public BocaSessionManager::Observer {
 public:
  using ActivityInterceptorCallback =
      base::OnceCallback<void(std::vector<mojom::IdentifiedActivityPtr>)>;
  using SessionConfigInterceptorCallback =
      base::OnceCallback<void(mojom::ConfigResultPtr)>;
  BocaAppHandler(
      mojo::PendingReceiver<mojom::PageHandler> receiver,
      mojo::PendingRemote<mojom::Page> remote,
      content::WebUI* webui,
      std::unique_ptr<WebviewAuthHandler> auth_handler,
      std::unique_ptr<ClassroomPageHandlerImpl> classroom_client_impl,
      std::unique_ptr<ContentSettingsHandler> content_settings_handler,
      SessionClientImpl* session_client_impl,
      bool is_producer);

  BocaAppHandler(const BocaAppHandler&) = delete;
  BocaAppHandler& operator=(const BocaAppHandler&) = delete;

  ~BocaAppHandler() override;
  // Static
  static void SetFloatModeAndBoundsForWindow(bool isFloatMode,
                                             aura::Window* window,
                                             SetFloatModeCallback callback);

  // mojom::PageHandler:
  void AuthenticateWebview(AuthenticateWebviewCallback callback) override;
  void GetWindowsTabsList(GetWindowsTabsListCallback callback) override;
  void ListCourses(ListCoursesCallback callback) override;
  void ListStudents(const std::string& course_id,
                    ListStudentsCallback callback) override;
  void ListAssignments(const std::string& course_id,
                       ListAssignmentsCallback callback) override;
  void CreateSession(mojom::ConfigPtr config,
                     CreateSessionCallback callback) override;
  void GetSession(GetSessionCallback callback) override;
  void EndSession(EndSessionCallback callback) override;
  void ExtendSessionDuration(base::TimeDelta extended_duration,
                             ExtendSessionDurationCallback callback) override;
  void RemoveStudent(const std::string& id,
                     RemoveStudentCallback callback) override;
  void UpdateOnTaskConfig(mojom::OnTaskConfigPtr config,
                          UpdateOnTaskConfigCallback callback) override;
  void UpdateCaptionConfig(mojom::CaptionConfigPtr config,
                           UpdateCaptionConfigCallback callback) override;
  void SetFloatMode(bool is_float_mode, SetFloatModeCallback callback) override;

  void SubmitAccessCode(const std::string& access_code,
                        SubmitAccessCodeCallback callback) override;

  void ViewStudentScreen(const std::string& id,
                         ViewStudentScreenCallback callback) override;
  void EndViewScreenSession(const std::string& id,
                            EndViewScreenSessionCallback callback) override;
  void GetUserPref(mojom::BocaValidPref pref,
                   GetUserPrefCallback callback) override;
  void SetUserPref(mojom::BocaValidPref pref,
                   base::Value value,
                   SetUserPrefCallback callback) override;
  void SetSitePermission(const std::string& url,
                         mojom::Permission permission,
                         mojom::PermissionSetting setting,
                         SetSitePermissionCallback callback) override;

  // mojom::Page:
  void OnStudentActivityUpdated(
      std::vector<mojom::IdentifiedActivityPtr> activities) override;
  void OnSessionConfigUpdated(mojom::ConfigResultPtr config) override;
  void OnActiveNetworkStateChanged(
      std::vector<mojom::NetworkInfoPtr> active_networks) override;

  // BocaSessionManager::Observer
  void OnConsumerActivityUpdated(
      const std::map<std::string, ::boca::StudentStatus>& activities) override;

  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnBundleUpdated(const ::boca::Bundle& bundle) override;
  void OnSessionCaptionConfigUpdated(
      const std::string& group_name,
      const ::boca::CaptionsConfig& config,
      const std::string& tachyon_group_id) override;
  void OnSessionRosterUpdated(const ::boca::Roster& roster) override;

  void NotifyLocalCaptionConfigUpdate(mojom::CaptionConfigPtr config);

  void SetSpotlightService(SpotlightService* spotlight_service);

  // For testing.
  // Mojo service binding is not invoked in unit test. So we manually override
  // a interceptor for testing.
  void SetActivityInterceptorCallbackForTesting(
      ActivityInterceptorCallback callback);
  void SetSessionConfigInterceptorCallbackForTesting(
      SessionConfigInterceptorCallback callback);
  void SetSpotlightServiceForTesting(std::unique_ptr<SpotlightService> service);
  WebviewAuthHandler* GetWebviewAuthHandlerForTesting() {
    return auth_handler_.get();
  }
  void SetPrefForTesting(PrefService* pref_service) {
    pref_service_ = pref_service;
  }

 private:
  void UpdateSessionConfig();
  void OnUpdatedOnTaskConfig(UpdateOnTaskConfigCallback callback,
                             base::expected<std::unique_ptr<::boca::Session>,
                                            google_apis::ApiErrorCode> result);
  void OnUpdatedCaptionConfig(UpdateCaptionConfigCallback callback,
                              base::expected<std::unique_ptr<::boca::Session>,
                                             google_apis::ApiErrorCode> result);
  void OnStudentRemoved(RemoveStudentCallback callback,
                        ::boca::Session* current_session,
                        std::string id,
                        base::expected<bool, google_apis::ApiErrorCode> result);

  void OnAccessCodeSubmitted(SubmitAccessCodeCallback callback,
                             base::expected<std::unique_ptr<::boca::Session>,
                                            google_apis::ApiErrorCode> result);

  SEQUENCE_CHECKER(sequence_checker_);
  const bool is_producer_;
  std::string base_url_;
  TabInfoCollector tab_info_collector_;
  std::unique_ptr<WebviewAuthHandler> auth_handler_;
  std::unique_ptr<ClassroomPageHandlerImpl> class_room_page_handler_;
  const std::unique_ptr<ContentSettingsHandler> content_settings_handler_;
  // Latest config is not always the same as the instance maintained in
  // boca_session_manager as it contains the async config that hasn't been
  // committed yet. OnTask and caption config use the same server endpoint. We
  // keep track of pending config to avoid override in race.
  std::unique_ptr<::boca::OnTaskConfig> latest_ontask_config_;
  std::unique_ptr<::boca::CaptionsConfig> latest_caption_config_;
  std::unique_ptr<NetworkInfoProvider> network_info_provider_;
  // Track the identity of the current app user.
  ::boca::UserIdentity user_identity_;
  mojo::Receiver<boca::mojom::PageHandler> receiver_;
  mojo::Remote<boca::mojom::Page> remote_;
  ActivityInterceptorCallback test_activity_callback_;
  SessionConfigInterceptorCallback test_config_callback_;
  raw_ptr<SpotlightService> spotlight_service_;
  raw_ptr<SessionClientImpl> session_client_impl_;
  raw_ptr<content::WebUI> web_ui_;
  raw_ptr<PrefService> pref_service_;
  base::WeakPtrFactory<BocaAppHandler> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
