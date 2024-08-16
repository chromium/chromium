// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_CLASSROOM_PAGE_HANDLER_IMPL_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_CLASSROOM_PAGE_HANDLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"

namespace mojom = ash::boca::mojom;
using ListCoursesCallback =
    base::OnceCallback<void(std::vector<mojom::CoursePtr>)>;
using ListStudentsCallback =
    base::OnceCallback<void(std::vector<mojom::IdentityPtr>)>;

namespace google_apis {
class RequestSender;

namespace classroom {
class Courses;
class Students;
}  // namespace classroom
}  // namespace google_apis

namespace ash::boca {

using StudentList = std::vector<mojom::IdentityPtr>;
using CourseList = std::vector<mojom::CoursePtr>;

class ClassroomPageHandlerImpl {
 public:
  ClassroomPageHandlerImpl();
  ClassroomPageHandlerImpl(std::unique_ptr<google_apis::RequestSender> sender);

  ClassroomPageHandlerImpl(const ClassroomPageHandlerImpl&) = delete;
  ClassroomPageHandlerImpl& operator=(const ClassroomPageHandlerImpl&) = delete;

  ~ClassroomPageHandlerImpl();

  void ListCourses(const std::string& teacherId, ListCoursesCallback callback);

  void ListStudents(const std::string& courseId, ListStudentsCallback callback);

 private:
  void ListCoursesHelper(const std::string& teacher_id,
                         const std::string& page_token,
                         std::unique_ptr<CourseList> fetched_courses,
                         ListCoursesCallback callback);

  void OnListCoursesFetched(
      const std::string& teacher_id,
      std::unique_ptr<CourseList> fetched_courses,
      ListCoursesCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                     google_apis::ApiErrorCode> result);

  void ListStudentsHelper(const std::string& course_id,
                          const std::string& page_token,
                          std::unique_ptr<StudentList> fetched_students,
                          ListStudentsCallback callback);

  void OnListStudentsFetched(
      const std::string& course_id,
      std::unique_ptr<StudentList> fetched_students,
      ListStudentsCallback callback,
      base::expected<std::unique_ptr<google_apis::classroom::Students>,
                     google_apis::ApiErrorCode> result);

  static std::unique_ptr<google_apis::RequestSender> CreateRequestSender();

  std::unique_ptr<google_apis::RequestSender> sender_;
  std::set<std::string> valid_course_ids_;
  base::WeakPtrFactory<ClassroomPageHandlerImpl> weak_factory_;
};

}  // namespace ash::boca

#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_CLASSROOM_PAGE_HANDLER_IMPL_H_
