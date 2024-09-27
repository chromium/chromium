// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/classroom/classroom_api_courses_response_types.h"
#include "google_apis/classroom/classroom_api_list_courses_request.h"
#include "google_apis/classroom/classroom_api_list_students_request.h"
#include "google_apis/classroom/classroom_api_students_response_types.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_constants.h"

namespace ash::boca {
namespace {
using ::google_apis::ApiErrorCode;
using ::google_apis::classroom::ListCoursesRequest;
using ::google_apis::classroom::ListStudentsRequest;

// TODO(b/343731449): Update this once policy to control has been added.
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("boca_classroom_integration", R"(
          semantics: {
            sender: "Boca"
            description: "Provide ChromeOS users easy access to roster and "
                         "courselist to control their in-classroom experience."
            internal {
              contacts {
                  email: "cros-edu-eng@google.com"
              }
            }
            user_data {
              type: ACCESS_TOKEN
              type: EMAIL
              type: NAME
            }
            trigger: "User opens Boca app and goes through session start flow."
            data: "The request is authenticated with an OAuth2 access token "
                  "identifying the Google account. Rosters and courselists "
                  "containing emails and names are sent to Boca."
            destination: GOOGLE_OWNED_SERVICE
            last_reviewed: "2024-06-26"
          }
          policy: {
            cookies_allowed: NO
            setting: "This feature cannot be disabled by settings yet."
            policy_exception_justification: "Not implemented yet."
          })");
}  // namespace

ClassroomPageHandlerImpl::ClassroomPageHandlerImpl(
    std::unique_ptr<google_apis::RequestSender> sender)
    : sender_(std::move(sender)), weak_factory_(this) {}

ClassroomPageHandlerImpl::ClassroomPageHandlerImpl()
    : ClassroomPageHandlerImpl(CreateRequestSender()) {}

ClassroomPageHandlerImpl::~ClassroomPageHandlerImpl() = default;

void ClassroomPageHandlerImpl::ListCourses(const std::string& teacher_id,
                                           ListCoursesCallback callback) {
  valid_course_ids_.clear();
  ListCoursesHelper(teacher_id, /*page_token=*/"",
                    std::make_unique<CourseList>(), std::move(callback));
}

void ClassroomPageHandlerImpl::ListStudents(const std::string& course_id,
                                            ListStudentsCallback callback) {
  if (valid_course_ids_.find(course_id) == valid_course_ids_.end()) {
    return std::move(callback).Run(StudentList());
  }
  ListStudentsHelper(course_id, /*page_token=*/"",
                     std::make_unique<StudentList>(), std::move(callback));
}

void ClassroomPageHandlerImpl::ListCoursesHelper(
    const std::string& teacher_id,
    const std::string& page_token,
    std::unique_ptr<CourseList> fetched_courses,
    ListCoursesCallback callback) {
  sender_->StartRequestWithAuthRetry(std::make_unique<ListCoursesRequest>(
      sender_.get(), /*student_id=*/"", teacher_id, page_token,
      base::BindOnce(&ClassroomPageHandlerImpl::OnListCoursesFetched,
                     weak_factory_.GetWeakPtr(), teacher_id,
                     std::move(fetched_courses), std::move(callback))));
}

void ClassroomPageHandlerImpl::OnListCoursesFetched(
    const std::string& teacher_id,
    std::unique_ptr<CourseList> fetched_courses,
    ListCoursesCallback callback,
    base::expected<std::unique_ptr<google_apis::classroom::Courses>,
                   ApiErrorCode> result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::move(*fetched_courses));
    return;
  }

  for (const auto& item : result.value()->items()) {
    mojom::CoursePtr course = mojom::Course::New(item->id(), item->name());
    fetched_courses->push_back(std::move(course));
    valid_course_ids_.insert(item->id());
  }

  if (!result.value()->next_page_token().empty()) {
    ListCoursesHelper(teacher_id, result.value()->next_page_token(),
                      std::move(fetched_courses), std::move(callback));
  } else {
    std::move(callback).Run(std::move(*fetched_courses));
  }
}

void ClassroomPageHandlerImpl::ListStudentsHelper(
    const std::string& course_id,
    const std::string& page_token,
    std::unique_ptr<StudentList> fetched_students,
    ListStudentsCallback callback) {
  sender_->StartRequestWithAuthRetry(std::make_unique<ListStudentsRequest>(
      sender_.get(), course_id, page_token,
      base::BindOnce(&ClassroomPageHandlerImpl::OnListStudentsFetched,
                     weak_factory_.GetWeakPtr(), course_id,
                     std::move(fetched_students), std::move(callback))));
}

void ClassroomPageHandlerImpl::OnListStudentsFetched(
    const std::string& course_id,
    std::unique_ptr<StudentList> fetched_students,
    ListStudentsCallback callback,
    base::expected<std::unique_ptr<google_apis::classroom::Students>,
                   ApiErrorCode> result) {
  if (!result.has_value()) {
    std::move(callback).Run(std::move(*fetched_students));
    return;
  }

  for (const auto& item : result.value()->items()) {
    mojom::IdentityPtr student = mojom::Identity::New(
        item->profile().id(), item->profile().name().full_name(),
        item->profile().email_address(), item->profile().photo_url());

    fetched_students->push_back(std::move(student));
  }

  if (!result.value()->next_page_token().empty()) {
    ListStudentsHelper(course_id, result.value()->next_page_token(),
                       std::move(fetched_students), std::move(callback));
  } else {
    std::move(callback).Run(std::move(*fetched_students));
  }
}

// static
std::unique_ptr<google_apis::RequestSender>
ClassroomPageHandlerImpl::CreateRequestSender() {
  std::vector<std::string> scopes = {
      GaiaConstants::kClassroomReadOnlyRostersOAuth2Scope,
      GaiaConstants::kClassroomReadOnlyCoursesOAuth2Scope,
      GaiaConstants::kClassroomProfileEmailOauth2Scope,
      GaiaConstants::kClassroomProfilePhotoUrlScope,
  };
  auto url_loader_factory = BocaAppClient::Get()->GetURLLoaderFactory();
  auto* identity_manager = BocaAppClient::Get()->GetIdentityManager();
  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory, scopes);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
               to the user on System UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/"", kTrafficAnnotation);
}

}  // namespace ash::boca
