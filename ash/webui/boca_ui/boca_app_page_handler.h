// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca {

class BocaUI;

class BocaAppHandler : public boca::mojom::PageHandler {
 public:
  BocaAppHandler(BocaUI* boca_ui,
                 mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
                 mojo::PendingRemote<boca::mojom::Page> remote,
                 content::WebUI* webui);

  BocaAppHandler(const BocaAppHandler&) = delete;
  BocaAppHandler& operator=(const BocaAppHandler&) = delete;

  ~BocaAppHandler() override;

  // boca::mojom::PageHandler:
  void GetWindowsTabsList(GetWindowsTabsListCallback callback) override;
  void ListCourses(ListCoursesCallback callback) override;
  void ListStudents(const std::string& course_id,
                    ListStudentsCallback callback) override;

 private:
  TabInfoCollector tab_info_collector_;
  ClassroomPageHandlerImpl class_room_page_handler_;
  mojo::Receiver<boca::mojom::PageHandler> receiver_;
  mojo::Remote<boca::mojom::Page> remote_;
  raw_ptr<BocaUI> boca_ui_;  // Owns |this|.
};
}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_BOCA_APP_PAGE_HANDLER_H_
