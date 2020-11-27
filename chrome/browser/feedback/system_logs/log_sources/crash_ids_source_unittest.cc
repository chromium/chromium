// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/upload_list/upload_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/fake_debug_daemon_client.h"
#endif

namespace system_logs {

class StubUploadList : public UploadList {
 public:
  StubUploadList() = default;
  StubUploadList(const StubUploadList&) = delete;
  StubUploadList& operator=(const StubUploadList&) = delete;

 protected:
  ~StubUploadList() override = default;
  std::vector<UploadInfo> LoadUploadList() override { return {}; }

  void ClearUploadList(const base::Time& begin,
                       const base::Time& end) override {}

  void RequestSingleUpload(const std::string& local_id) override {}
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestDebugDaemonClient : public chromeos::FakeDebugDaemonClient {
 public:
  TestDebugDaemonClient() = default;
  ~TestDebugDaemonClient() override = default;

  void UploadCrashes(UploadCrashesCallback callback) override {
    ++upload_crashes_called_;
    FakeDebugDaemonClient::UploadCrashes(std::move(callback));
  }

  int upload_crashes_called() const { return upload_crashes_called_; }

 private:
  int upload_crashes_called_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestDebugDaemonClient);
};

TestDebugDaemonClient* fake_debug_client() {
  return static_cast<TestDebugDaemonClient*>(
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient());
}

TEST(CrashIdsSourceTest, CallsCrashSender) {
  content::BrowserTaskEnvironment task_environment;

  auto setter = chromeos::DBusThreadManager::GetSetterForTesting();
  setter->SetDebugDaemonClient(std::make_unique<TestDebugDaemonClient>());

  CrashIdsSource source;
  source.SetUploadListForTesting(new StubUploadList());

  EXPECT_EQ(0, fake_debug_client()->upload_crashes_called());

  source.Fetch(base::BindOnce([](std::unique_ptr<SystemLogsResponse>) {}));

  EXPECT_EQ(1, fake_debug_client()->upload_crashes_called());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace system_logs
