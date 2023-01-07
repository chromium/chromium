// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader_impl.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class IncidentReportUploaderImplTest : public testing::Test {
 public:
  // safe_browsing::IncidentReportUploader::OnResultCallback implementation.
  void OnReportUploadResult(IncidentReportUploader::Result result,
                            std::unique_ptr<ClientIncidentResponse> response) {
    result_ = result;
    response_ = std::move(response);
  }

 protected:
  IncidentReportUploaderImplTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        current_default_handle_(task_runner_),
        result_(IncidentReportUploader::UPLOAD_REQUEST_FAILED) {}

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;
  IncidentReportUploader::Result result_;
  std::unique_ptr<ClientIncidentResponse> response_;
};

TEST_F(IncidentReportUploaderImplTest, Success) {
  network::TestURLLoaderFactory test_url_loader_factory;
  auto url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);

  ClientIncidentReport report;
  auto instance(IncidentReportUploaderImpl::UploadReport(
      base::BindOnce(&IncidentReportUploaderImplTest::OnReportUploadResult,
                     base::Unretained(this)),
      url_loader_factory, report));

  std::string response;
  ClientIncidentResponse().SerializeToString(&response);

  static_cast<IncidentReportUploaderImpl*>(instance.get())
      ->OnURLLoaderCompleteInternal(response, net::HTTP_OK, net::OK);

  EXPECT_EQ(IncidentReportUploader::UPLOAD_SUCCESS, result_);
  EXPECT_TRUE(response_);
}

}  // namespace safe_browsing

// TODO(grt):
// bad status/response code
// confirm data in request is in upload test
// confirm data in response is parsed
