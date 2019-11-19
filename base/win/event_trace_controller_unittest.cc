// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for event trace controller.

#include <objbase.h>
#include <initguid.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/win/event_trace_controller.h"
#include "base/win/event_trace_provider.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

DEFINE_GUID(kGuidNull,
    0x0000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0);

const ULONG kTestProviderFlags = 0xCAFEBABE;

class TestingProvider: public EtwTraceProvider {
 public:
  explicit TestingProvider(const GUID& provider_name)
      : EtwTraceProvider(provider_name) {
    callback_event_.Set(::CreateEvent(NULL, TRUE, FALSE, NULL));
  }

  void WaitForCallback() {
    ::WaitForSingleObject(callback_event_.Get(), INFINITE);
    ::ResetEvent(callback_event_.Get());
  }

 private:
  void OnEventsEnabled() override { ::SetEvent(callback_event_.Get()); }
  void PostEventsDisabled() override { ::SetEvent(callback_event_.Get()); }

  ScopedHandle callback_event_;

  DISALLOW_COPY_AND_ASSIGN(TestingProvider);
};

}  // namespace

TEST(EtwTracePropertiesTest, Initialization) {
  EtwTraceProperties prop;

  EVENT_TRACE_PROPERTIES* p = prop.get();
  EXPECT_NE(0u, p->Wnode.BufferSize);
  EXPECT_EQ(0u, p->Wnode.ProviderId);
  EXPECT_EQ(0u, p->Wnode.HistoricalContext);

  EXPECT_TRUE(kGuidNull == p->Wnode.Guid);
  EXPECT_EQ(0u, p->Wnode.ClientContext);
  EXPECT_EQ(static_cast<ULONG>(WNODE_FLAG_TRACED_GUID), p->Wnode.Flags);

  EXPECT_EQ(0u, p->BufferSize);
  EXPECT_EQ(0u, p->MinimumBuffers);
  EXPECT_EQ(0u, p->MaximumBuffers);
  EXPECT_EQ(0u, p->MaximumFileSize);
  EXPECT_EQ(0u, p->LogFileMode);
  EXPECT_EQ(0u, p->FlushTimer);
  EXPECT_EQ(0u, p->EnableFlags);
  EXPECT_EQ(0, p->AgeLimit);

  EXPECT_EQ(0u, p->NumberOfBuffers);
  EXPECT_EQ(0u, p->FreeBuffers);
  EXPECT_EQ(0u, p->EventsLost);
  EXPECT_EQ(0u, p->BuffersWritten);
  EXPECT_EQ(0u, p->LogBuffersLost);
  EXPECT_EQ(0u, p->RealTimeBuffersLost);
  EXPECT_EQ(0u, p->LoggerThreadId);
  EXPECT_NE(0u, p->LogFileNameOffset);
  EXPECT_NE(0u, p->LoggerNameOffset);
}

TEST(EtwTracePropertiesTest, Strings) {
  EtwTraceProperties prop;

  ASSERT_STREQ(L"", prop.GetLoggerFileName());
  ASSERT_STREQ(L"", prop.GetLoggerName());

  std::wstring name(1023, L'A');
  ASSERT_HRESULT_SUCCEEDED(prop.SetLoggerFileName(name.c_str()));
  ASSERT_HRESULT_SUCCEEDED(prop.SetLoggerName(name.c_str()));
  ASSERT_STREQ(name.c_str(), prop.GetLoggerFileName());
  ASSERT_STREQ(name.c_str(), prop.GetLoggerName());

  std::wstring name2(1024, L'A');
  ASSERT_HRESULT_FAILED(prop.SetLoggerFileName(name2.c_str()));
  ASSERT_HRESULT_FAILED(prop.SetLoggerName(name2.c_str()));
}

namespace {

class EtwTraceControllerTest : public testing::Test {
 public:
  EtwTraceControllerTest()
      : session_name_(StringPrintf(L"TestSession-%d", GetCurrentProcId())) {
  }

  void SetUp() override {
    EtwTraceProperties ignore;
    EtwTraceController::Stop(session_name_.c_str(), &ignore);

    // Allocate a new provider name GUID for each test.
    ASSERT_HRESULT_SUCCEEDED(::CoCreateGuid(&test_provider_));
  }

  void TearDown() override {
    EtwTraceProperties prop;
    EtwTraceController::Stop(session_name_.c_str(), &prop);
  }

 protected:
  GUID test_provider_;
  std::wstring session_name_;
};

}  // namespace

TEST_F(EtwTraceControllerTest, Initialize) {
  EtwTraceController controller;

  EXPECT_EQ(0u, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}


TEST_F(EtwTraceControllerTest, StartRealTimeSession) {
  EtwTraceController controller;

  HRESULT hr = controller.StartRealtimeSession(session_name_.c_str(),
                                               100 * 1024);
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_NE(0u, controller.session());
  EXPECT_STREQ(session_name_.c_str(), controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(0u, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
}

TEST_F(EtwTraceControllerTest, StartFileSession) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath temp;
  ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir.GetPath(), &temp));

  EtwTraceController controller;
  HRESULT hr = controller.StartFileSession(session_name_.c_str(),
                                           as_wcstr(temp.value()));
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    DeleteFile(temp, false);
    return;
  }

  EXPECT_NE(0u, controller.session());
  EXPECT_STREQ(session_name_.c_str(), controller.session_name());

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));
  EXPECT_EQ(0u, controller.session());
  EXPECT_STREQ(L"", controller.session_name());
  DeleteFile(temp, false);
}

// This test is flaky for unclear reasons. See bugs 525297 and 534184
TEST_F(EtwTraceControllerTest, DISABLED_EnableDisable) {
  TestingProvider provider(test_provider_);

  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS), provider.Register());
  EXPECT_EQ(0u, provider.session_handle());

  EtwTraceController controller;
  HRESULT hr = controller.StartRealtimeSession(session_name_.c_str(),
                                               100 * 1024);
  if (hr == E_ACCESSDENIED) {
    VLOG(1) << "You must be an administrator to run this test on Vista";
    return;
  }

  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(test_provider_,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  provider.WaitForCallback();

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  EXPECT_HRESULT_SUCCEEDED(controller.DisableProvider(test_provider_));

  provider.WaitForCallback();

  EXPECT_EQ(0, provider.enable_level());
  EXPECT_EQ(0u, provider.enable_flags());

  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS), provider.Unregister());

  // Enable the provider again, before registering.
  EXPECT_HRESULT_SUCCEEDED(controller.EnableProvider(test_provider_,
                           TRACE_LEVEL_VERBOSE, kTestProviderFlags));

  // Register the provider again, the settings above
  // should take immediate effect.
  EXPECT_EQ(static_cast<DWORD>(ERROR_SUCCESS), provider.Register());

  EXPECT_EQ(TRACE_LEVEL_VERBOSE, provider.enable_level());
  EXPECT_EQ(kTestProviderFlags, provider.enable_flags());

  // Consume the callback event of the previous controller.EnableProvider().
  provider.WaitForCallback();

  EXPECT_HRESULT_SUCCEEDED(controller.Stop(NULL));

  // Windows 7 does not call the callback when Stop() is called so we
  // can't wait, and enable_level and enable_flags are not zeroed.
  if (GetVersion() >= Version::WIN8) {
    provider.WaitForCallback();

    // Session should have wound down.
    EXPECT_EQ(0, provider.enable_level());
    EXPECT_EQ(0u, provider.enable_flags());
  }
}

}  // namespace win
}  // namespace base
