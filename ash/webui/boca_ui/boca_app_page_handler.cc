// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/boca_app_page_handler.h"

#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/provider/tab_info_collector.h"
#include "content/public/browser/web_ui.h"

namespace {
// Special filter value for `ListCoursesRequest` to request courses with access
// limited to the requesting user.
constexpr char kOwnCoursesFilterValue[] = "me";
}  // namespace

namespace ash::boca {

BocaAppHandler::BocaAppHandler(
    BocaUI* boca_ui,
    mojo::PendingReceiver<boca::mojom::PageHandler> receiver,
    mojo::PendingRemote<boca::mojom::Page> remote,
    content::WebUI* web_ui)
    : tab_info_collector_(web_ui),
      receiver_(this, std::move(receiver)),
      remote_(std::move(remote)),
      boca_ui_(boca_ui) {}

BocaAppHandler::~BocaAppHandler() = default;

void BocaAppHandler::GetWindowsTabsList(GetWindowsTabsListCallback callback) {
  tab_info_collector_.GetWindowTabInfo(std::move(callback));
}

void BocaAppHandler::ListCourses(ListCoursesCallback callback) {
  class_room_page_handler_.ListCourses(kOwnCoursesFilterValue,
                                       std::move(callback));
}

void BocaAppHandler::ListStudents(const std::string& course_id,
                                  ListStudentsCallback callback) {
  class_room_page_handler_.ListStudents(course_id, std::move(callback));
}
}  // namespace ash::boca
