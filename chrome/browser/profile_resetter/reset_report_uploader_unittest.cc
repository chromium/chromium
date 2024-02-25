// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/reset_report_uploader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ResetReportUploaderTest : public testing::Test {
 public:
  ResetReportUploaderTest()
      : test_shared_loader_factory_(
            test_url_loader_factory_.GetSafeWeakWrapper()) {}

 protected:
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return test_shared_loader_factory_;
  }
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
};

TEST_F(ResetReportUploaderTest, NoCrash) {
  test_url_loader_factory()->AddResponse(
      ResetReportUploader::GetClientReportUrlForTesting().spec(), "");
  ResetReportUploader* uploader =
      new ResetReportUploader(shared_url_loader_factory());
  uploader->DispatchReportInternal("");
}
