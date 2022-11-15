// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::printing::oauth2 {
namespace {

class PrintingOAuth2ClientIdsDatabaseTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState pref_ =
      ScopedTestingLocalState(TestingBrowserProcess::GetGlobal());
  std::unique_ptr<ClientIdsDatabase> client_ids_database_ =
      ClientIdsDatabase::Create();
};

// Represents results returned by callback void(StatusCode, std::string).
struct CallbackResult {
  StatusCode status = StatusCode::kUnexpectedError;
  std::string data;
};

TEST_F(PrintingOAuth2ClientIdsDatabaseTest, FetchEmptyDatabase) {
  base::MockOnceCallback<void(StatusCode, std::string)> callback;
  CallbackResult result;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run)
      .WillOnce([&result, &loop](StatusCode status, std::string data) {
        result.status = status;
        result.data = std::move(data);
        loop.Quit();
      });
  client_ids_database_->FetchId(GURL("https://abc/d"), callback.Get());
  loop.Run();

  EXPECT_EQ(result.status, StatusCode::kOK);
  EXPECT_TRUE(result.data.empty());
}

TEST_F(PrintingOAuth2ClientIdsDatabaseTest, StoreAndFetch) {
  client_ids_database_->StoreId(GURL("https://abc/d"), "my_id");

  base::MockOnceCallback<void(StatusCode, std::string)> callback;
  CallbackResult result;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run)
      .WillOnce([&result, &loop](StatusCode status, std::string data) {
        result.status = status;
        result.data = std::move(data);
        loop.Quit();
      });
  client_ids_database_->FetchId(GURL("https://abc/d"), callback.Get());
  loop.Run();

  EXPECT_EQ(result.status, StatusCode::kOK);
  EXPECT_EQ(result.data, "my_id");
}

}  // namespace
}  // namespace ash::printing::oauth2
