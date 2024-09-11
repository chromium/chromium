// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_

#include <memory>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "chromeos/ash/components/boca/session_api/session_client_impl.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {

class BocaUI;

class BocaAppHandler : public mojom::PageHandler {
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

  void NotifyLocalConfigUpdate(mojom::ConfigPtr config);

 private:
  TabInfoCollector tab_info_collector_;
  std::unique_ptr<ClassroomPageHandlerImpl> class_room_page_handler_;
  std::unique_ptr<SessionClientImpl> session_client_impl_;
  // Track the identity of the current app user.
  AccountId user_identity_;
  mojo::Receiver<boca::mojom::PageHandler> receiver_;
  mojo::Remote<boca::mojom::Page> remote_;
  raw_ptr<BocaUI> boca_ui_;  // Owns |this|.
};
}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
