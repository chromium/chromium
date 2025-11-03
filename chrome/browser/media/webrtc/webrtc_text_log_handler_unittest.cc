// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_text_log_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/media/webrtc/webrtc_log_buffer.h"
#include "chrome/browser/media/webrtc/webrtc_log_uploader.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/webrtc_logging/common/partial_circular_buffer.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

namespace webrtc_text_log {

class WebRtcTextLogHandlerTest : public testing::Test {
 public:
  WebRtcTextLogHandlerTest() = default;

  ~WebRtcTextLogHandlerTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);
#endif
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(
        std::make_unique<WebRtcLogUploader>());
    text_log_handler_ = std::make_unique<WebRtcTextLogHandler>(1);
  }

  void TearDown() override {
    if (text_log_handler_->GetState() != WebRtcTextLogHandler::CLOSED) {
      text_log_handler_->ChannelClosing();
      text_log_handler_->DiscardLog();
    }
    text_log_handler_.reset();

    WebRtcLogUploader* uploader =
        TestingBrowserProcess::GetGlobal()->webrtc_log_uploader();
    if (uploader) {
      uploader->Shutdown();
    }
    TestingBrowserProcess::GetGlobal()->SetWebRtcLogUploader(nullptr);
#if BUILDFLAG(IS_CHROMEOS)
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
#endif
  }

  // Helper to get the handler to STARTED state.
  void StartLogging() {
    base::RunLoop run_loop;
    bool lambda_success = false;
    std::string lambda_error_message = "Callback not executed";

    ASSERT_TRUE(text_log_handler_->StartLogging(base::BindLambdaForTesting(
        [&](bool success, const std::string& error_message) {
          lambda_success = success;
          lambda_error_message = error_message;
          run_loop.Quit();
        })));
    run_loop.Run();

    ASSERT_TRUE(lambda_success) << lambda_error_message;
    ASSERT_TRUE(lambda_error_message.empty())
        << "Error message should be empty";
    ASSERT_EQ(text_log_handler_->GetState(),
              WebRtcTextLogHandler::LoggingState::STARTED);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<WebRtcTextLogHandler> text_log_handler_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::system::FakeStatisticsProvider fake_statistics_provider_;
#endif
};

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenClosed) {
  EXPECT_EQ(text_log_handler_->GetState(), WebRtcTextLogHandler::CLOSED);

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_TRUE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenStarting) {
  // StartLogging is async. The state becomes STARTING synchronously.
  text_log_handler_->StartLogging(base::DoNothing());
  EXPECT_EQ(text_log_handler_->GetState(), WebRtcTextLogHandler::STARTING);

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_TRUE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenStarted) {
  ASSERT_NO_FATAL_FAILURE(StartLogging());
  EXPECT_EQ(text_log_handler_->GetState(), WebRtcTextLogHandler::STARTED);

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_FALSE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenStopping) {
  ASSERT_NO_FATAL_FAILURE(StartLogging());
  text_log_handler_->StopLogging(base::DoNothing());
  EXPECT_EQ(text_log_handler_->GetState(), WebRtcTextLogHandler::STOPPING);

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_TRUE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenStopped) {
  ASSERT_NO_FATAL_FAILURE(StartLogging());
  base::RunLoop run_loop;
  text_log_handler_->StopLogging(
      base::BindLambdaForTesting([&](bool success, const std::string& error) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  // StopLogging is async, but StopDone is what moves it to STOPPED.
  // We need to call it manually.
  text_log_handler_->StopDone();
  run_loop.Run();
  EXPECT_EQ(text_log_handler_->GetState(), WebRtcTextLogHandler::STOPPED);

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_TRUE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, GetLogMessageCallbackWhenChannelIsClosing) {
  ASSERT_NO_FATAL_FAILURE(StartLogging());
  text_log_handler_->ChannelClosing();
  EXPECT_TRUE(text_log_handler_->GetChannelIsClosing());

  auto callback = text_log_handler_->GetLogMessageCallback();

  EXPECT_TRUE(callback.is_null());
}

TEST_F(WebRtcTextLogHandlerTest, LogMessageCallbackWritesToBuffer) {
  ASSERT_NO_FATAL_FAILURE(StartLogging());
  auto callback = text_log_handler_->GetLogMessageCallback();
  ASSERT_FALSE(callback.is_null());

  const std::string test_message = "Hello, world!";
  callback.Run(test_message);

  // Wait for the logging to happen on the sequence.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Stop logging to be able to release the buffer.
  base::RunLoop stop_run_loop;
  text_log_handler_->StopLogging(base::BindLambdaForTesting(
      [&](bool success, const std::string& error_message) {
        ASSERT_TRUE(success);
        ASSERT_TRUE(error_message.empty());
        stop_run_loop.Quit();
      }));
  text_log_handler_->StopDone();
  stop_run_loop.Run();

  // Release and check buffer.
  std::unique_ptr<WebRtcLogBuffer> log_buffer;
  std::unique_ptr<WebRtcLogMetaDataMap> meta_data;
  text_log_handler_->ReleaseLog(&log_buffer, &meta_data);

  ASSERT_TRUE(log_buffer);
  webrtc_logging::PartialCircularBuffer read_buffer = log_buffer->Read();

  std::string buffer_content;
  buffer_content.resize(kWebRtcLogSize);
  read_buffer.Read(reinterpret_cast<uint8_t*>(&buffer_content[0]),
                   kWebRtcLogSize);

  // The buffer contains startup info, then our message.
  // Let's just check if our message is present.
  EXPECT_THAT(buffer_content, testing::HasSubstr(test_message));
}

}  // namespace webrtc_text_log
