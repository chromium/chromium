// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_ui/provider/classroom_page_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/dummy_auth_service.h"
#include "google_apis/common/test_util.h"
#include "google_apis/common/time_util.h"
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
#include "url/gurl.h"

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
  base::HistogramTester histogram_tester_;
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
                  "section": "Period 1",
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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListCourses(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 2u);
  EXPECT_EQ(response.at(0)->id, "course-id-1");
  EXPECT_EQ(response.at(0)->name, "Course 1");
  EXPECT_EQ(response.at(0)->section, "Period 1");
  EXPECT_EQ(response.at(1)->id, "course-id-2");
  EXPECT_EQ(response.at(1)->name, "Course 2");
  // Empty string when section is missing.
  EXPECT_EQ(response.at(1)->section, "");
}

TEST_F(ClassroomPageHandlerImplTest, ListCoursesOnHttpError) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(request_handler(), HandleRequest(testing::_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  std::vector<mojom::CoursePtr> response;
  base::MockCallback<ListCoursesCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListCourses(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();
  histogram_tester.ExpectTotalCount("Ash.Boca.ListCourses.ErrorCode", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Boca.ListCourses.ErrorCode",
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 1);
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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        response = std::move(courses);
      });

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

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
      .WillOnce([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      });

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(request_handler(), HandleRequest(testing::_))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 0u);
  histogram_tester.ExpectTotalCount("Ash.Boca.ListStudents.ErrorCode", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Boca.ListStudents.ErrorCode",
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 1);
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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

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
      .WillOnce([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      });

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  std::vector<mojom::IdentityPtr> response;
  base::MockCallback<ListStudentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&](std::vector<mojom::IdentityPtr> students) {
        response = std::move(students);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListStudents(
      "foo",
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  ASSERT_EQ(response.size(), 0u);
}

class ClassroomPageHandlerImplTestWithFlag
    : public ClassroomPageHandlerImplTest,
      public testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kBocaCourseWorkMaterialApi);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kBocaCourseWorkMaterialApi);
    }
    ClassroomPageHandlerImplTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ClassroomPageHandlerImplTestWithFlag, ListAllAssignments) {
  const bool is_material_api_enabled = GetParam();

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  const char kOriginalCourseWorkResponse[] = R"({
      "courseWork": [
        {
          "id": "assignment-multiple-materials-id",
          "title": "assignment-multiple-materials-title",
          "alternateLink": "http://assignment-multiple-materials-url.com",
          "workType": "ASSIGNMENT",
          "updateTime": "2025-01-01T00:00:00.000Z",
          "materials": [
            { "driveFile": { "driveFile": { "title": "drive-file-title" } } },
            { "youtubeVideo": { "title": "youtube-video-title" } }
          ]
        },
        {
          "id": "assignment-link-materials-id",
          "title": "assignment-link-materials-title",
          "alternateLink": "http://assignment-link-materials-url.com",
          "workType": "ASSIGNMENT",
          "updateTime": "2025-01-02T01:02:03.400Z",
          "materials": [ { "link": { "title": "link-title" } } ]
        },
        {
          "id": "assignment-form-materials-id",
          "title": "assignment-form-materials-title",
          "alternateLink": "http://assignment-form-materials-url.com",
          "workType": "ASSIGNMENT",
          "updateTime": "2025-02-03T02:03:04.500Z",
          "materials": [ { "form": { "title": "form-title" } } ]
        },
        {
          "id": "assignment-unknown-materials-id",
          "title": "assignment-unknown-materials-title",
          "alternateLink": "http://assignment-unknown-materials-url.com",
          "workType": "ASSIGNMENT",
          "updateTime": "2025-03-04T03:04:05.600Z",
          "materials": [ { "unknownType": {} } ]
        },
        {
          "id": "short-answer-question-id",
          "title": "short-answer-question-title",
          "alternateLink": "http://short-answer-question-url.com",
          "workType": "SHORT_ANSWER_QUESTION",
          "updateTime": "2025-04-05T04:05:06.700Z"
        },
        {
          "id": "multiple-choice-question-id",
          "title": "multiple-choice-question-title",
          "alternateLink": "http://multiple-choice-question-url.com",
          "workType": "MULTIPLE_CHOICE_QUESTION",
          "updateTime": "2025-04-05T04:05:06.700Z"
        },
        {
          "id": "type-unspecified-id",
          "title": "type-unspecified-title",
          "alternateLink": "http://type-unspecified-url.com",
          "workType": "COURSE_WORK_TYPE_UNSPECIFIED",
          "updateTime": "2025-04-05T04:05:06.700Z"
        },
        {
          "id": "no-type-id",
          "title": "no-type-title",
          "alternateLink": "http://no-type-url.com",
          "updateTime": "2025-04-05T04:05:06.700Z"
        }
      ]
  })";

  EXPECT_CALL(request_handler(),
              HandleRequest(
                  Field(&HttpRequest::relative_url, HasSubstr("/courseWork?"))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(
          kOriginalCourseWorkResponse))));

  if (is_material_api_enabled) {
    EXPECT_CALL(request_handler(),
                HandleRequest(Field(&HttpRequest::relative_url,
                                    HasSubstr("/courseWorkMaterials?"))))
        .WillOnce(
            Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
            "courseWorkMaterial": [
              {
                "id": "material-1",
                "title": "Syllabus Reading",
                "alternateLink": "http://coursework-material-url.com",
                "updateTime": "2025-05-06T05:06:07.800Z"
              }
            ]
        })"))));
  }

  std::vector<mojom::AssignmentPtr> response;
  base::MockCallback<ListAssignmentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([&](std::vector<mojom::AssignmentPtr> assignments) {
        response = std::move(assignments);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListAssignments(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  const size_t expected_base_size = 6u;
  const size_t expected_total_size =
      is_material_api_enabled ? expected_base_size + 1 : expected_base_size;
  ASSERT_EQ(response.size(), expected_total_size);

  EXPECT_EQ(response.at(0)->title, "assignment-multiple-materials-title");
  EXPECT_EQ(response.at(0)->url,
            GURL("http://assignment-multiple-materials-url.com"));
  EXPECT_EQ(response.at(0)->type, mojom::AssignmentType::kAssignment);
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(0)->last_update_time),
      "2025-01-01T00:00:00.000Z");
  EXPECT_EQ(response.at(0)->materials.size(), 2u);
  EXPECT_EQ(response.at(0)->materials.at(0)->title, "drive-file-title");
  EXPECT_EQ(response.at(0)->materials.at(0)->type,
            mojom::MaterialType::kSharedDriveFile);
  EXPECT_EQ(response.at(0)->materials.at(1)->title, "youtube-video-title");
  EXPECT_EQ(response.at(0)->materials.at(1)->type,
            mojom::MaterialType::kYoutubeVideo);

  EXPECT_EQ(response.at(1)->title, "assignment-link-materials-title");
  EXPECT_EQ(response.at(1)->url,
            GURL("http://assignment-link-materials-url.com"));
  EXPECT_EQ(response.at(1)->type, mojom::AssignmentType::kAssignment);
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(1)->last_update_time),
      "2025-01-02T01:02:03.400Z");

  EXPECT_EQ(response.at(1)->materials.size(), 1u);
  EXPECT_EQ(response.at(1)->materials.at(0)->title, "link-title");
  EXPECT_EQ(response.at(1)->materials.at(0)->type, mojom::MaterialType::kLink);

  EXPECT_EQ(response.at(2)->title, "assignment-form-materials-title");
  EXPECT_EQ(response.at(2)->url,
            GURL("http://assignment-form-materials-url.com"));
  EXPECT_EQ(response.at(2)->type, mojom::AssignmentType::kAssignment);
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(2)->last_update_time),
      "2025-02-03T02:03:04.500Z");
  EXPECT_EQ(response.at(2)->materials.size(), 1u);
  EXPECT_EQ(response.at(2)->materials.at(0)->title, "form-title");
  EXPECT_EQ(response.at(2)->materials.at(0)->type, mojom::MaterialType::kForm);

  EXPECT_EQ(response.at(3)->title, "assignment-unknown-materials-title");
  EXPECT_EQ(response.at(3)->type, mojom::AssignmentType::kAssignment);
  EXPECT_EQ(response.at(3)->url,
            GURL("http://assignment-unknown-materials-url.com"));
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(3)->last_update_time),
      "2025-03-04T03:04:05.600Z");
  EXPECT_EQ(response.at(3)->materials.size(), 1u);
  EXPECT_EQ(response.at(3)->materials.at(0)->type,
            mojom::MaterialType::kUnknown);

  EXPECT_EQ(response.at(4)->title, "short-answer-question-title");
  EXPECT_EQ(response.at(4)->url, GURL("http://short-answer-question-url.com"));
  EXPECT_EQ(response.at(4)->type, mojom::AssignmentType::kShortAnswerQuestion);
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(3)->last_update_time),
      "2025-03-04T03:04:05.600Z");

  EXPECT_EQ(response.at(5)->title, "multiple-choice-question-title");
  EXPECT_EQ(response.at(5)->url,
            GURL("http://multiple-choice-question-url.com"));
  EXPECT_EQ(response.at(5)->type,
            mojom::AssignmentType::kMultipleChoiceQuestion);
  EXPECT_EQ(
      google_apis::util::FormatTimeAsString(response.at(3)->last_update_time),
      "2025-03-04T03:04:05.600Z");

  if (is_material_api_enabled) {
    EXPECT_EQ(response.at(6)->title, "Syllabus Reading");
    EXPECT_EQ(response.at(6)->url, GURL("http://coursework-material-url.com"));
    EXPECT_EQ(response.at(6)->type, mojom::AssignmentType::kUnspecified);
    EXPECT_EQ(
        google_apis::util::FormatTimeAsString(response.at(6)->last_update_time),
        "2025-05-06T05:06:07.800Z");
  }
}

TEST_P(ClassroomPageHandlerImplTestWithFlag, ListAssignmentsOnHttpError) {
  const bool is_material_api_enabled = GetParam();

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  base::HistogramTester histogram_tester;
  if (is_material_api_enabled) {
    EXPECT_CALL(request_handler(),
                HandleRequest(Field(&HttpRequest::relative_url,
                                    HasSubstr("/courseWork?"))))
        .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));
    EXPECT_CALL(request_handler(),
                HandleRequest(Field(&HttpRequest::relative_url,
                                    HasSubstr("/courseWorkMaterials?"))))
        .WillOnce(
            Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
          "courseWorkMaterial": [
            {
              "id": "material-1",
              "title": "Material Only"
            }
          ]
        })"))));
  } else {
    EXPECT_CALL(request_handler(),
                HandleRequest(Field(&HttpRequest::relative_url,
                                    HasSubstr("/courseWork?"))))
        .WillOnce(Return(ByMove(TestRequestHandler::CreateFailedResponse())));
  }

  std::vector<mojom::AssignmentPtr> response;
  base::MockCallback<ListAssignmentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([&](std::vector<mojom::AssignmentPtr> assignments) {
        response = std::move(assignments);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListAssignments(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  histogram_tester.ExpectTotalCount("Ash.Boca.ListCourseWorks.ErrorCode", 1);
  histogram_tester.ExpectBucketCount(
      "Ash.Boca.ListCourseWorks.ErrorCode",
      google_apis::ApiErrorCode::HTTP_INTERNAL_SERVER_ERROR, 1);

  if (is_material_api_enabled) {
    ASSERT_EQ(response.size(), 1u);
    EXPECT_EQ(response.at(0)->title, "Material Only");
  } else {
    ASSERT_EQ(response.size(), 0u);
  }
}

TEST_P(ClassroomPageHandlerImplTestWithFlag, ListAssignmentsMultiplePages) {
  const bool is_material_api_enabled = GetParam();

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
      .WillOnce([&](std::vector<mojom::CoursePtr> courses) {
        course_response = std::move(courses);
      });

  base::RunLoop course_run_loop;
  classroom_handler()->ListCourses(
      "foo", google_apis::test_util::CreateQuitCallback(&course_run_loop,
                                                        course_callback.Get()));
  course_run_loop.Run();

  // Mock a 3-page response from the /courseWork endpoint.
  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        Not(HasSubstr("pageToken"))))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "id-page-1",
                  "title": "title-page-1",
                  "alternateLink": "http://url-page-1.com",
                  "workType": "ASSIGNMENT"
                }
              ],
              "nextPageToken": "page-2-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-2-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "id-page-2",
                  "title": "title-page-2",
                  "alternateLink": "http://url-page-2.com",
                  "workType": "ASSIGNMENT"
                }
              ],
              "nextPageToken": "page-3-token"
            })"))));

  EXPECT_CALL(request_handler(),
              HandleRequest(Field(&HttpRequest::relative_url,
                                  AllOf(HasSubstr("/courseWork?"),
                                        HasSubstr("pageToken=page-3-token")))))
      .WillOnce(Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"(
            {
              "courseWork": [
                {
                  "id": "id-page-3",
                  "title": "title-page-3",
                  "alternateLink": "http://url-page-3.com",
                  "workType": "ASSIGNMENT"
                }
              ]
            })"))));

  if (is_material_api_enabled) {
    // If the flag is on, also mock a 2-page response from /courseWorkMaterials.
    EXPECT_CALL(request_handler(),
                HandleRequest(Field(&HttpRequest::relative_url,
                                    AllOf(HasSubstr("/courseWorkMaterials?"),
                                          Not(HasSubstr("pageToken"))))))
        .WillOnce(
            Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
            "courseWorkMaterial": [{
              "id": "id-material-1",
              "title": "mat-title-p1",
              "alternateLink": "http://url-material-1.com"
            }],
            "nextPageToken": "page-2-token"
        })"))));
    EXPECT_CALL(
        request_handler(),
        HandleRequest(Field(&HttpRequest::relative_url,
                            AllOf(HasSubstr("/courseWorkMaterials?"),
                                  HasSubstr("pageToken=page-2-token")))))
        .WillOnce(
            Return(ByMove(TestRequestHandler::CreateSuccessfulResponse(R"({
            "courseWorkMaterial": [{
              "id": "id-material-2",
              "title": "mat-title-p2",
              "alternateLink": "http://url-material-2.com"
            }]
        })"))));
  }

  std::vector<mojom::AssignmentPtr> response;
  base::MockCallback<ListAssignmentsCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce([&](std::vector<mojom::AssignmentPtr> assignments) {
        response = std::move(assignments);
      });

  base::RunLoop run_loop;
  classroom_handler()->ListAssignments(
      course_response.at(0)->id,
      google_apis::test_util::CreateQuitCallback(&run_loop, callback.Get()));
  run_loop.Run();

  if (is_material_api_enabled) {
    // Expect 3 items from /courseWork + 2 from /courseWorkMaterials.
    ASSERT_EQ(response.size(), 5u);

    // Assertions for /courseWork items.
    EXPECT_EQ(response.at(0)->title, "title-page-1");
    EXPECT_EQ(response.at(0)->url, GURL("http://url-page-1.com"));
    EXPECT_EQ(response.at(0)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(0)->materials.empty());

    EXPECT_EQ(response.at(1)->title, "title-page-2");
    EXPECT_EQ(response.at(1)->url, GURL("http://url-page-2.com"));
    EXPECT_EQ(response.at(1)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(1)->materials.empty());

    EXPECT_EQ(response.at(2)->title, "title-page-3");
    EXPECT_EQ(response.at(2)->url, GURL("http://url-page-3.com"));
    EXPECT_EQ(response.at(2)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(2)->materials.empty());

    // Assertions for /courseWorkMaterials items.
    EXPECT_EQ(response.at(3)->title, "mat-title-p1");
    EXPECT_EQ(response.at(3)->url, GURL("http://url-material-1.com"));
    EXPECT_EQ(response.at(3)->type, mojom::AssignmentType::kUnspecified);
    EXPECT_TRUE(response.at(3)->materials.empty());

    EXPECT_EQ(response.at(4)->title, "mat-title-p2");
    EXPECT_EQ(response.at(4)->url, GURL("http://url-material-2.com"));
    EXPECT_EQ(response.at(4)->type, mojom::AssignmentType::kUnspecified);
    EXPECT_TRUE(response.at(4)->materials.empty());
  } else {
    // Expect only the 3 items from /courseWork.
    ASSERT_EQ(response.size(), 3u);

    EXPECT_EQ(response.at(0)->title, "title-page-1");
    EXPECT_EQ(response.at(0)->url, GURL("http://url-page-1.com"));
    EXPECT_EQ(response.at(0)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(0)->materials.empty());

    EXPECT_EQ(response.at(1)->title, "title-page-2");
    EXPECT_EQ(response.at(1)->url, GURL("http://url-page-2.com"));
    EXPECT_EQ(response.at(1)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(1)->materials.empty());

    EXPECT_EQ(response.at(2)->title, "title-page-3");
    EXPECT_EQ(response.at(2)->url, GURL("http://url-page-3.com"));
    EXPECT_EQ(response.at(2)->type, mojom::AssignmentType::kAssignment);
    EXPECT_TRUE(response.at(2)->materials.empty());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClassroomPageHandlerImplTestWithFlag,
                         testing::Bool());

}  // namespace ash::boca
