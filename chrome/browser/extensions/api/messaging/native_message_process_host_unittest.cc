// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/page_size.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_test_util.h"
#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "net/base/file_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/win/scoped_handle.h"
#else
#include <unistd.h>
#endif

namespace {

const char kTestMessage[] = "{\"text\": \"Hello.\"}";

}  // namespace

namespace extensions {

class FakeLauncher : public NativeProcessLauncher {
 public:
  static std::unique_ptr<NativeProcessLauncher> Create(
      base::FilePath read_file,
      base::FilePath write_file) {
    int read_flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
    int write_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE;
#if !BUILDFLAG(IS_POSIX)
    read_flags |= base::File::FLAG_ASYNC;
    write_flags |= base::File::FLAG_ASYNC;
#endif
    return std::unique_ptr<NativeProcessLauncher>(new FakeLauncher(
        CreateBackgroundTaskRunner(), base::File(read_file, read_flags),
        base::File(write_file, write_flags)));
  }

  static std::unique_ptr<NativeProcessLauncher> CreateWithPipeInput(
      base::File read_pipe,
      base::FilePath write_file) {
    int write_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE;
#if !BUILDFLAG(IS_POSIX)
    write_flags |= base::File::FLAG_ASYNC;
#endif

    return std::unique_ptr<NativeProcessLauncher>(
        new FakeLauncher(CreateBackgroundTaskRunner(), std::move(read_pipe),
                         base::File(write_file, write_flags)));
  }

  void Launch(const GURL& origin,
              const std::string& native_host_name,
              LaunchedCallback callback) const override {
    std::move(callback).Run(
        NativeProcessLauncher::RESULT_SUCCESS, base::Process(),
        std::exchange(read_file_, base::kInvalidPlatformFile),
        std::move(read_stream_), std::move(write_stream_));
  }

 private:
  FakeLauncher(scoped_refptr<base::TaskRunner> task_runner,
               base::File read_file,
               base::File write_file)
      : read_file_(read_file.GetPlatformFile()),
        read_stream_(std::make_unique<net::FileStream>(std::move(read_file),
                                                       task_runner)),
        write_stream_(std::make_unique<net::FileStream>(std::move(write_file),
                                                        task_runner)) {}

  static scoped_refptr<base::TaskRunner> CreateBackgroundTaskRunner() {
    return base::ThreadPool::CreateTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()});
  }

  mutable base::PlatformFile read_file_;
  mutable std::unique_ptr<net::FileStream> read_stream_;
  mutable std::unique_ptr<net::FileStream> write_stream_;
};

class NativeMessagingTest : public ::testing::Test,
                            public NativeMessageHost::Client {
 protected:
  NativeMessagingTest()
      : current_channel_(version_info::Channel::DEV),
        task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    if (native_message_host_) {
      content::GetIOThreadTaskRunner({})->DeleteSoon(
          FROM_HERE, native_message_host_.release());
    }
    base::RunLoop().RunUntilIdle();
  }

  void PostMessageFromNativeHost(const std::string& message) override {
    last_message_ = message;

    // Parse the message.
    std::optional<base::Value> dict_value = base::JSONReader::Read(message);
    if (!dict_value || !dict_value->is_dict()) {
      LOG(ERROR) << "Failed to parse " << message;
      last_message_parsed_.reset();
    } else {
      last_message_parsed_ = std::move(*dict_value).TakeDict();
    }

    if (run_loop_)
      run_loop_->Quit();
  }

  void CloseChannel(const std::string& error_message) override {
    channel_closed_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

 protected:
  std::string FormatMessage(const std::string& message) {
    uint32_t length = message.length();
    return std::string(reinterpret_cast<char*>(&length), 4).append(message);
  }

  base::FilePath CreateTempFileWithMessage(const std::string& message) {
    base::FilePath filename;
    if (!base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &filename))
      return base::FilePath();

    std::string message_with_header = FormatMessage(message);
    if (!base::WriteFile(filename, message_with_header))
      return base::FilePath();

    return filename;
  }

  base::ScopedTempDir temp_dir_;
  // Force the channel to be dev.
  ScopedCurrentChannel current_channel_;
  std::unique_ptr<NativeMessageHost> native_message_host_;
  std::unique_ptr<base::RunLoop> run_loop_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::string last_message_;
  std::optional<base::Value::Dict> last_message_parsed_;
  bool channel_closed_ = false;
};

// Read a single message from a local file.
TEST_F(NativeMessagingTest, SingleSendMessageRead) {
  base::FilePath temp_output_file = temp_dir_.GetPath().AppendASCII("output");
#if BUILDFLAG(IS_WIN)
  base::FilePath temp_input_file = CreateTempFileWithMessage(kTestMessage);
  ASSERT_FALSE(temp_input_file.empty());
  std::unique_ptr<NativeProcessLauncher> launcher =
      FakeLauncher::Create(temp_input_file, temp_output_file);
#else   // BUILDFLAG(IS_WIN)
  base::PlatformFile pipe_handles[2];
  ASSERT_EQ(0, pipe(pipe_handles));
  base::File read_file(pipe_handles[0]);
  std::string formatted_message = FormatMessage(kTestMessage);
  ASSERT_GT(base::GetPageSize(), formatted_message.size());
  ASSERT_TRUE(base::WriteFileDescriptor(pipe_handles[1], formatted_message));
  base::File write_file(pipe_handles[1]);
  std::unique_ptr<NativeProcessLauncher> launcher =
      FakeLauncher::CreateWithPipeInput(std::move(read_file), temp_output_file);
#endif  // BUILDFLAG(IS_WIN)
  native_message_host_ = NativeMessageProcessHost::CreateWithLauncher(
      ScopedTestNativeMessagingHost::kExtensionId, "empty_app.py",
      std::move(launcher));
  ASSERT_TRUE(last_message_.empty());
  native_message_host_->Start(this);

  ASSERT_TRUE(native_message_host_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  EXPECT_EQ(kTestMessage, last_message_);
}

// Tests sending a single message. The message should get written to
// |temp_file| and should match the contents of single_message_request.msg.
TEST_F(NativeMessagingTest, SingleSendMessageWrite) {
  base::FilePath temp_output_file = temp_dir_.GetPath().AppendASCII("output");

  base::File read_file;
#if BUILDFLAG(IS_WIN)
  std::wstring pipe_name = base::ASCIIToWide(base::StringPrintf(
      "\\\\.\\pipe\\chrome.nativeMessaging.out.%llx", base::RandUint64()));
  base::File write_handle =
      base::File(base::ScopedPlatformFile(CreateNamedPipeW(
                     pipe_name.c_str(),
                     PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED |
                         FILE_FLAG_FIRST_PIPE_INSTANCE,
                     PIPE_TYPE_BYTE, 1, 0, 0, 5000, nullptr)),
                 true /* async */);
  ASSERT_TRUE(write_handle.IsValid());
  base::File read_handle =
      base::File(base::ScopedPlatformFile(CreateFileW(
                     pipe_name.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr)),
                 true /* async */);
  ASSERT_TRUE(read_handle.IsValid());

  read_file = std::move(read_handle);
#else   // BUILDFLAG(IS_WIN)
  base::PlatformFile pipe_handles[2];
  ASSERT_EQ(0, pipe(pipe_handles));
  read_file = base::File(pipe_handles[0]);
  base::File write_file(pipe_handles[1]);
#endif  // !BUILDFLAG(IS_WIN)

  std::unique_ptr<NativeProcessLauncher> launcher =
      FakeLauncher::CreateWithPipeInput(std::move(read_file), temp_output_file);
  native_message_host_ = NativeMessageProcessHost::CreateWithLauncher(
      ScopedTestNativeMessagingHost::kExtensionId, "empty_app.py",
      std::move(launcher));
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);
  base::RunLoop().RunUntilIdle();

  native_message_host_->OnMessage(kTestMessage);
  base::RunLoop().RunUntilIdle();

  std::string output;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while (base::TimeTicks::Now() - start_time < TestTimeouts::action_timeout()) {
    ASSERT_TRUE(base::ReadFileToString(temp_output_file, &output));
    if (!output.empty())
      break;
    base::PlatformThread::YieldCurrentThread();
  }

  EXPECT_EQ(FormatMessage(kTestMessage), output);
}

// Test send message with a real client. The client just echo's back the text
// it received.
TEST_F(NativeMessagingTest, EchoConnect) {
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));
  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::kHostName, false, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);

  native_message_host_->OnMessage("{\"text\": \"Hello.\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);

  std::string expected_url = std::string("chrome-extension://") +
                             ScopedTestNativeMessagingHost::kExtensionId + "/";

  {
    std::optional<int> id = last_message_parsed_->FindInt("id");
    ASSERT_TRUE(id);
    EXPECT_EQ(1, *id);
    const std::string* text =
        last_message_parsed_->FindStringByDottedPath("echo.text");
    ASSERT_TRUE(text);
    EXPECT_EQ("Hello.", *text);
    const std::string* url = last_message_parsed_->FindString("caller_url");
    EXPECT_TRUE(url);
    EXPECT_EQ(expected_url, *url);
  }

  native_message_host_->OnMessage("{\"foo\": \"bar\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  {
    std::optional<int> id = last_message_parsed_->FindInt("id");
    ASSERT_TRUE(id);
    EXPECT_EQ(2, *id);
    const std::string* text =
        last_message_parsed_->FindStringByDottedPath("echo.foo");
    ASSERT_TRUE(text);
    EXPECT_EQ("bar", *text);
    const std::string* url = last_message_parsed_->FindString("caller_url");
    ASSERT_TRUE(url);
    EXPECT_EQ(expected_url, *url);
  }

  const base::Value* args = last_message_parsed_->Find("args");
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->is_none());

  const base::Value* connect_id_value =
      last_message_parsed_->Find("connect_id");
  ASSERT_TRUE(connect_id_value);
  EXPECT_TRUE(connect_id_value->is_none());
}

// Test send message with a real client. The args passed when launching the
// native messaging host should contain reconnect args.
TEST_F(NativeMessagingTest, ReconnectArgs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kOnConnectNative);
  ScopedAllowNativeAppConnectionForTest allow_native_app_connection(true);
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));
  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      false, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);

  native_message_host_->OnMessage("{\"text\": \"Hello.\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);

  const base::Value::List* args_value = last_message_parsed_->FindList("args");
  ASSERT_TRUE(args_value);
  std::vector<base::CommandLine::StringType> args;
  args.reserve(args_value->size());
  for (auto& arg : *args_value) {
    ASSERT_TRUE(arg.is_string());
#if BUILDFLAG(IS_WIN)
    args.push_back(base::UTF8ToWide(arg.GetString()));
#else
    args.push_back(arg.GetString());
#endif
  }
  base::CommandLine cmd_line(args);
  base::FilePath exe_path;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &exe_path));
  EXPECT_EQ(exe_path, cmd_line.GetProgram());
  EXPECT_TRUE(cmd_line.HasSwitch(switches::kNoStartupWindow));
  EXPECT_EQ(
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      cmd_line.GetSwitchValueASCII(switches::kNativeMessagingConnectHost));
  EXPECT_EQ(
      ScopedTestNativeMessagingHost::kExtensionId,
      cmd_line.GetSwitchValueASCII(switches::kNativeMessagingConnectExtension));
  EXPECT_EQ(features::kOnConnectNative.name,
            cmd_line.GetSwitchValueASCII(switches::kEnableFeatures));
  EXPECT_EQ(profile_.GetBaseName(),
            cmd_line.GetSwitchValuePath(switches::kProfileDirectory));
  EXPECT_EQ(profile_.GetPath().DirName(),
            cmd_line.GetSwitchValuePath(switches::kUserDataDir));
}

// Test send message with a real client. The args passed when launching the
// native messaging host should not contain reconnect args.
TEST_F(NativeMessagingTest, ReconnectArgs_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOnConnectNative);
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));
  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      false, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);

  native_message_host_->OnMessage("{\"text\": \"Hello.\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);

  const base::Value* args = last_message_parsed_->Find("args");
  ASSERT_TRUE(args);
  EXPECT_TRUE(args->is_none());
}

// Test that reconnect args are not sent if the extension is not permitted to
// receive natively-established connections.
TEST_F(NativeMessagingTest, ReconnectArgsIfNativeConnectionDisallowed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kOnConnectNative);
  ScopedAllowNativeAppConnectionForTest disallow_native_app_connection(false);
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(false));
  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::
          kSupportsNativeInitiatedConnectionsHostName,
      false, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);

  native_message_host_->OnMessage("{\"text\": \"Hello.\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);

  const base::Value* args_value = last_message_parsed_->Find("args");
  ASSERT_TRUE(args_value);
  EXPECT_TRUE(args_value->is_none());

  const base::Value* connect_id_value =
      last_message_parsed_->Find("connect_id");
  ASSERT_TRUE(connect_id_value);
  EXPECT_TRUE(connect_id_value->is_none());
}

TEST_F(NativeMessagingTest, UserLevel) {
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(true));

  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::kHostName, true, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);

  native_message_host_->OnMessage("{\"text\": \"Hello.\"}");
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  ASSERT_FALSE(last_message_.empty());
  ASSERT_TRUE(last_message_parsed_);
}

TEST_F(NativeMessagingTest, DisallowUserLevel) {
  ScopedTestNativeMessagingHost test_host;
  ASSERT_NO_FATAL_FAILURE(test_host.RegisterTestHost(true));

  std::string error_message;
  native_message_host_ = NativeMessageProcessHost::Create(
      &profile_, NULL, ScopedTestNativeMessagingHost::kExtensionId,
      ScopedTestNativeMessagingHost::kHostName, false, &error_message);
  native_message_host_->Start(this);
  ASSERT_TRUE(native_message_host_);
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();

  // The host should fail to start.
  ASSERT_TRUE(channel_closed_);
}

}  // namespace extensions
