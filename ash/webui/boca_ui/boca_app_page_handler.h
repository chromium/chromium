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
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {

class BocaUI;

class BocaAppHandler : public mojom::PageHandler, public mojom::Page {
 public:
  BocaAppHandler(
      BocaUI* boca_ui,
      mojo::PendingReceiver<mojom::PageHandler> receiver,
      mojo::PendingRemote<mojom::Page> remote,
      content::WebUI* webui,
      std::unique_ptr<ClassroomPageHandlerImpl> classroom_client_impl,
      std::unique_ptr<SessionClientImpl> session_client_impl);

  BocaAppHandler(const BocaAppHandler&) = delete;
  BocaAppHandler& operator=(const BocaAppHandler&) = delete;

  ~BocaAppHandler() override;

  // mojom::PageHandler:
  void GetWindowsTabsList(GetWindowsTabsListCallback callback) override;
  void ListCourses(ListCoursesCallback callback) override;
  void ListStudents(const std::string& course_id,
                    ListStudentsCallback callback) override;
  void CreateSession(mojom::ConfigPtr config,
                     CreateSessionCallback callback) override;
  void GetSession(GetSessionCallback callback) override;
  void EndSession(EndSessionCallback callback) override;
  void UpdateOnTaskConfig(mojom::OnTaskConfigPtr config,
                          UpdateOnTaskConfigCallback callback) override;
  void UpdateCaptionConfig(mojom::CaptionConfigPtr config,
                           UpdateCaptionConfigCallback callback) override;

  void OnStudentActivityUpdated(
      std::vector<mojom::IdentifiedActivityPtr> activities) override {}

  void OnSessionConfigUpdated(mojom::ConfigPtr config) override {}

  void NotifyLocalCaptionConfigUpdate(mojom::CaptionConfigPtr config);

 private:
  void OnUpdatedOnTaskConfig(UpdateOnTaskConfigCallback callback,
                             base::expected<std::unique_ptr<::boca::Session>,
                                            google_apis::ApiErrorCode> result);
  void OnUpdatedCaptionConfig(UpdateCaptionConfigCallback callback,
                              base::expected<std::unique_ptr<::boca::Session>,
                                             google_apis::ApiErrorCode> result);

  SEQUENCE_CHECKER(sequence_checker_);
  TabInfoCollector tab_info_collector_;
  std::unique_ptr<ClassroomPageHandlerImpl> class_room_page_handler_;
  std::unique_ptr<SessionClientImpl> session_client_impl_;
  // Lastest config is not always the same as the instance maintained in
  // boca_session_manager as it contains the async config that hasn't been
  // committed yet. OnTask and caption config use the same server endpoint. We
  // keep track of pending config to avoid override in race.
  std::unique_ptr<::boca::OnTaskConfig> latest_ontask_config_;
  std::unique_ptr<::boca::CaptionsConfig> latest_caption_config_;
  // Track the identity of the current app user.
  ::boca::UserIdentity user_identity_;
  mojo::Receiver<boca::mojom::PageHandler> receiver_;
  mojo::Remote<boca::mojom::Page> remote_;
  raw_ptr<BocaUI> boca_ui_;  // Owns |this|.
  base::WeakPtrFactory<BocaAppHandler> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
