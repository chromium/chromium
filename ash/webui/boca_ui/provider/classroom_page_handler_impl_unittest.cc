// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"

#include "base/command_line.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/test_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::Return;

// Helper class to simplify mocking `net::EmbeddedTestServer` responses,
// especially useful for subsequent responses when testing pagination logic.
class TestRequestHandler {
 public:
  static std::unique_ptr<HttpResponse> CreateSuccessfulResponse(
      const std::string& content) {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(::net::HTTP_OK);
    response->set_content(content);
    response->set_content_type("application/json");
    return response;
  }

  static std::unique_ptr<HttpResponse> CreateFailedResponse() {
    auto response = std::make_unique<BasicHttpResponse>();
    response->set_code(::net::HTTP_INTERNAL_SERVER_ERROR);
    return response;
  }

  MOCK_METHOD(std::unique_ptr<HttpResponse>,
              HandleRequest,
              (const HttpRequest&));
};

}  // namespace

class ClassroomPageHandlerImplTest : public testing::Test {
 public:
  ClassroomPageHandlerImplTest()
      : url_loader_factory_(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
                /*network_service=*/nullptr,
                /*is_trusted=*/true)) {}

  void SetUp() override {
    classroom_handler_ =
        std::make_unique<ClassroomPageHandlerImpl>(MakeRequestSender());

    test_server_.RegisterRequestHandler(
        base::BindRepeating(&TestRequestHandler::HandleRequest,
                            base::Unretained(&request_handler_)));
    ASSERT_TRUE(test_server_.Start());

    gaia_urls_overrider_ = std::make_unique<GaiaUrlsOverriderForTesting>(
        base::CommandLine::ForCurrentProcess(), "classroom_api_origin_url",
        test_server_.base_url().spec());
    ASSERT_EQ(GaiaUrls::GetInstance()->classroom_api_origin_url(),
              test_server_.base_url().spec());
  }

  TestRequestHandler& request_handler() { return request_handler_; }
  ClassroomPageHandlerImpl* classroom_handler() {
    return classroom_handler_.get();
  }

 private:
  // Makes a request sender configured for testing.
  std::unique_ptr<google_apis::RequestSender> MakeRequestSender() {
    return std::make_unique<google_apis::RequestSender>(
        std::make_unique<google_apis::DummyAuthService>(), url_loader_factory_,
        task_environment_.GetMainThreadTaskRunner(), "test-user-agent",
        TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;
  std::unique_ptr<google_apis::RequestSender> request_sender_;
  scoped_refptr<network::TestSharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<GaiaUrlsOverriderForTesting> gaia_urls_overrider_;
  testing::StrictMock<TestRequestHandler> request_handler_;
  std::unique_ptr<ClassroomPageHandlerImpl> classroom_handler_;
};

TEST_F(ClassroomPageHandlerImplTest, ListAllCourses) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Course 1",
                  "courseState": "ACTIVE"
                },
                {
                  "id": "course-id-2",
                  "name": "Course 2",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  std::vector<mojom::CoursePtr> response;
  base::MockCallback<ListCoursesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListCourses(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response.at(0)->id, "course-id-1");
  EXPECT_EQ(response.at(0)->name, "Course 1");
  EXPECT_EQ(response.at(1)->id, "course-id-2");
  EXPECT_EQ(response.at(1)->name, "Course 2");
}

TEST_F(ClassroomPageHandlerImplTest, ListCoursesOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(testing::_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  std::vector<mojom::CoursePtr> response;
  base::MockCallback<ListCoursesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListCourses(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 0u);
}

TEST_F(ClassroomPageHandlerImplTest, ListCoursesMultiplePages) {
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/courses?"), Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-1", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-2-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-2", "courseState": "ACTIVE"}
              ],
              "nextPageToken": "page-3-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courses?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {"id": "course-id-from-page-3", "courseState": "ACTIVE"}
              ]
            })"))));

  std::vector<mojom::CoursePtr> response;
  base::MockCallback<ListCoursesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListCourses(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 3u);
  EXPECT_EQ(response.at(0)->id, "course-id-from-page-1");
  EXPECT_EQ(response.at(1)->id, "course-id-from-page-2");
  EXPECT_EQ(response.at(2)->id, "course-id-from-page-3");
}

TEST_F(ClassroomPageHandlerImplTest, ListAllStudents) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  std::vector<mojom::CoursePtr> course_response;
  base::MockCallback<ListCoursesCallback> course_callback;
  EXPECT_CALL(course_callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      }));

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/students?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
        {
          "students":[
              {
                "profile":{
                    "id":"student-1",
                    "name":{
                      "fullName":"Student1 full"
                    },
                    "emailAddress":"student1@foo.com",
                    "photoUrl":"//student1"
                }
              },
              {
                "profile":{
                    "id":"student-2",
                    "name":{
                      "fullName":"Student2 full"
                    },
                    "emailAddress":"student2@foo.com",
                    "photoUrl":"//student2"
                }
              }
          ]
        })"))));

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response.at(0)->id, "student-1");
  EXPECT_EQ(response.at(0)->name, "Student1 full");
  EXPECT_EQ(response.at(0)->email, "student1@foo.com");
  EXPECT_EQ(response.at(0)->photo_url.value().spec(), "https://student1/");
  EXPECT_EQ(response.at(1)->id, "student-2");
  EXPECT_EQ(response.at(1)->name, "Student2 full");
  EXPECT_EQ(response.at(1)->email, "student2@foo.com");
  EXPECT_EQ(response.at(1)->photo_url.value().spec(), "https://student2/");
}

TEST_F(ClassroomPageHandlerImplTest, ListStudentsOnHttpError) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  std::vector<mojom::CoursePtr> course_response;
  base::MockCallback<ListCoursesCallback> course_callback;
  EXPECT_CALL(course_callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      }));

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  EXPECT_CALL(request_handler(), HandleRequest(testing::_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 0u);
}

TEST_F(ClassroomPageHandlerImplTest, ListStudentsMultiplePages) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  std::vector<mojom::CoursePtr> course_response;
  base::MockCallback<ListCoursesCallback> course_callback;
  EXPECT_CALL(course_callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      }));

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(
                  &HttpRequest::relative_url,
                  AllOf(HasSubstr("/students?"), Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "students":[
              {
                 "profile":{
                    "id":"student-1-page-1",
                    "name":{
                       "fullName":"Student1 full"
                    },
                    "emailAddress":"student1@foo.com",
                    "photoUrl":"//student1"
                 }
              }
           ],
              "nextPageToken": "page-2-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/students?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "students":[
              {
                 "profile":{
                    "id":"student-2-page-2",
                    "name":{
                       "fullName":"Student2 full"
                    },
                    "emailAddress":"student2@foo.com",
                    "photoUrl":"//student2"
                 }
              }
           ],
              "nextPageToken": "page-3-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/students?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "students":[
                {
                  "profile":{
                      "id":"student-3-page-3",
                      "name":{
                        "fullName":"Student1 full"
                      },
                      "emailAddress":"student1@foo.com",
                                          "photoUrl":"https://student3"

                  }
                }
            ]
            })"))));

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 3u);
  EXPECT_EQ(response.at(0)->id, "student-1-page-1");
  EXPECT_EQ(response.at(1)->id, "student-2-page-2");
  EXPECT_EQ(response.at(2)->id, "student-3-page-3");
}

TEST_F(ClassroomPageHandlerImplTest, ListStudentsWithInvalidCourseId) {
  EXPECT_CALL(request_handler(), HandleRequest(Field(&HttpRequest::relative_url,
                                                     HasSubstr("/courses?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courses": [
                {
                  "id": "course-id-1",
                  "name": "Course 1",
                  "courseState": "ACTIVE"
                }
              ]
            })"))));

  std::vector<mojom::CoursePtr> course_response;
  base::MockCallback<ListCoursesCallback> course_callback;
  EXPECT_CALL(course_callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      }));

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      }));

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 0u);
}

}  // namespace ash::boca
