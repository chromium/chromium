// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/upload_list/upload_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#endif

namespace system_logs {

class StubUploadList : public UploadList {
 public:
  StubUploadList() = default;
  StubUploadList(const StubUploadList&) = delete;
  StubUploadList& operator=(const StubUploadList&) = delete;

 protected:
  ~StubUploadList() override = default;
  std::vector<std::unique_ptr<UploadInfo>> LoadUploadList() override {
    return {};
  }

  void ClearUploadList(const base::Time& begin,
                       const base::Time& end) override {}

  void RequestSingleUpload(const std::string& local_id) override {}
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestDebugDaemonClient : public ash::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;

  TestDebugDaemonClient(const TestDebugDaemonClient&) = delete;
  TestDebugDaemonClient& operator=(const TestDebugDaemonClient&) = delete;

  ~TestDebugDaemonClient() override = default;

  void UploadCrashes(UploadCrashesCallback callback) override {
    ++upload_crashes_called_;
    FakeDebugDaemonClient::UploadCrashes(std::move(callback));
  }

  int upload_crashes_called() const { return upload_crashes_called_; }

 private:
  int upload_crashes_called_ = 0;
};

TEST(CrashIdsSourceTest, CallsCrashSender) {
  content::BrowserTaskEnvironment task_environment;

  TestDebugDaemonClient test_debug_client;
  ash::DebugDaemonClient::SetInstanceForTest(&test_debug_client);

  CrashIdsSource source;
  source.SetUploadListForTesting(new StubUploadList());

  EXPECT_EQ(0, test_debug_client.upload_crashes_called());

  source.Fetch(base::BindOnce([](std::unique_ptr<SystemLogsResponse>) {}));

  EXPECT_EQ(1, test_debug_client.upload_crashes_called());

  ash::DebugDaemonClient::SetInstanceForTest(nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace system_logs
