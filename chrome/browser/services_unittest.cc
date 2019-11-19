// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "components/services/patch/content/patch_service.h"
#include "components/services/patch/public/mojom/file_patcher.mojom.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ServicesTest : public testing::Test {
 public:
  ServicesTest()
      : task_environment_(content::BrowserTaskEnvironment::MainThreadType::IO) {
  }

  template <typename Interface>
  bool IsConnected(mojo::Remote<Interface>* remote) {
    bool connected = true;
    remote->set_disconnect_handler(
        base::BindLambdaForTesting([&] { connected = false; }));
    remote->FlushForTesting();
    return connected;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  DISALLOW_COPY_AND_ASSIGN(ServicesTest);
};

}  // namespace

TEST_F(ServicesTest, ConnectToUnzip) {
  mojo::Remote<unzip::mojom::Unzipper> unzipper(unzip::LaunchUnzipper());
  EXPECT_TRUE(IsConnected(&unzipper));
}

TEST_F(ServicesTest, ConnectToFilePatch) {
  mojo::Remote<patch::mojom::FilePatcher> patcher(patch::LaunchFilePatcher());
  EXPECT_TRUE(IsConnected(&patcher));
}
