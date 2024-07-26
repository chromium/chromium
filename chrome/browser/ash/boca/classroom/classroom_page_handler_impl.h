// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_CLASSROOM_CLASSROOM_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_CLASSROOM_CLASSROOM_PAGE_HANDLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/boca/classroom/classroom.mojom.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace google_apis {
class RequestSender;

namespace classroom {
class Courses;
class Students;
}  // namespace classroom
}  // namespace google_apis

class Profile;

namespace ash {

using StudentList = std::vector<boca::classroom::mojom::StudentPtr>;
using CourseList = std::vector<boca::classroom::mojom::CoursePtr>;

class ClassroomPageHandlerImpl
    : public boca::classroom::mojom::ClassroomPageHandler {
 public:
  ClassroomPageHandlerImpl(
      mojo::PendingReceiver<boca::classroom::mojom::ClassroomPageHandler>
          receiver,
      Profile* profile);
  ClassroomPageHandlerImpl(
      mojo::PendingReceiver<boca::classroom::mojom::ClassroomPageHandler>
          receiver,
      Profile* profile,
      std::unique_ptr<google_apis::RequestSender> sender);

  ClassroomPageHandlerImpl(const ClassroomPageHandlerImpl&) = delete;
  ClassroomPageHandlerImpl& operator=(const ClassroomPageHandlerImpl&) = delete;

  ~ClassroomPageHandlerImpl() override;

  void ListCourses(const std::string& teacherId,
                   ListCoursesCallback callback) override;

  void ListStudents(const std::string& courseId,
                    ListStudentsCallback callback) override;

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

  static std::unique_ptr<google_apis::RequestSender> CreateRequestSender(
      Profile* profile);

  mojo::Receiver<boca::classroom::mojom::ClassroomPageHandler> receiver_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<google_apis::RequestSender> sender_;
  std::set<std::string> valid_course_ids_;
  base::WeakPtrFactory<ClassroomPageHandlerImpl> weak_factory_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_CLASSROOM_CLASSROOM_PAGE_HANDLER_IMPL_H_
