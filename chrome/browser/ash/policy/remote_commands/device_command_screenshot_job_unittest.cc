// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"

#include <map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// String constant identifying the result field in the result payload.
const char* const kResultFieldName = "result";

const char* const kMockUploadUrl = "http://example.com/upload";

const RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

// String constant identifying the upload url field in the command payload.
const char* const kUploadUrlFieldName = "fileUploadUrl";

em::RemoteCommand GenerateScreenshotCommandProto(
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeDelta age_of_command,
    const std::string upload_url) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_DEVICE_SCREENSHOT);
  command_proto.set_command_id(unique_id);
  command_proto.set_age_of_command(age_of_command.InMilliseconds());
  std::string payload;
  auto root_dict = base::Value::Dict().Set(kUploadUrlFieldName, upload_url);
  base::JSONWriter::Write(root_dict, &payload);
  command_proto.set_payload(payload);
  return command_proto;
}

class MockUploadJob : public UploadJob {
 public:
  // If |error_code| is a null pointer OnSuccess() will be invoked when the
  // Start() method is called, otherwise OnFailure() will be invoked with the
  // respective |error_code|.
  MockUploadJob(const GURL& upload_url,
                UploadJob::Delegate* delegate,
                std::unique_ptr<UploadJob::ErrorCode> error_code);
  ~MockUploadJob() override;

  // UploadJob:
  void AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      std::unique_ptr<std::string> data) override;
  void Start() override;

  const GURL& GetUploadUrl() const;

 protected:
  const GURL upload_url_;
  raw_ptr<UploadJob::Delegate> delegate_;
  std::unique_ptr<UploadJob::ErrorCode> error_code_;
  bool add_datasegment_succeeds_;
};

MockUploadJob::MockUploadJob(const GURL& upload_url,
                             UploadJob::Delegate* delegate,
                             std::unique_ptr<UploadJob::ErrorCode> error_code)
    : upload_url_(upload_url),
      delegate_(delegate),
      error_code_(std::move(error_code)) {}

MockUploadJob::~MockUploadJob() = default;

void MockUploadJob::AddDataSegment(
    const std::string& name,
    const std::string& filename,
    const std::map<std::string, std::string>& header_entries,
    std::unique_ptr<std::string> data) {}

void MockUploadJob::Start() {
  DCHECK(delegate_);
  EXPECT_EQ(kMockUploadUrl, upload_url_.spec());
  if (error_code_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&UploadJob::Delegate::OnFailure,
                                  base::Unretained(delegate_), *error_code_));
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UploadJob::Delegate::OnSuccess,
                                base::Unretained(delegate_)));
}

scoped_refptr<base::RefCountedBytes> GenerateTestPNG(const int& width,
                                                     const int& height) {
  const SkColor background_color = SK_ColorBLUE;
  SkBitmap bmp;
  bmp.allocN32Pixels(width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      *bmp.getAddr32(x, y) = background_color;
    }
  }
  scoped_refptr<base::RefCountedBytes> png_bytes(new base::RefCountedBytes());
  gfx::PNGCodec::ColorFormat color_format = gfx::PNGCodec::FORMAT_RGBA;
  if (!gfx::PNGCodec::Encode(
          reinterpret_cast<const unsigned char*>(bmp.getPixels()), color_format,
          gfx::Size(bmp.width(), bmp.height()),
          static_cast<int>(bmp.rowBytes()), false,
          std::vector<gfx::PNGCodec::Comment>(), &png_bytes->as_vector())) {
    LOG(ERROR) << "Failed to encode image";
  }
  return png_bytes;
}

class FakeScreenshotDelegate : public DeviceCommandScreenshotJob::Delegate {
 public:
  FakeScreenshotDelegate(
      std::unique_ptr<UploadJob::ErrorCode> upload_job_error_code,
      bool screenshot_allowed);
  ~FakeScreenshotDelegate() override;

  bool IsScreenshotAllowed() override;
  void TakeSnapshot(gfx::NativeWindow window,
                    const gfx::Rect& source_rect,
                    OnScreenshotTakenCallback callback) override;
  std::unique_ptr<UploadJob> CreateUploadJob(const GURL&,
                                             UploadJob::Delegate*) override;

 private:
  std::unique_ptr<UploadJob::ErrorCode> upload_job_error_code_;
  bool screenshot_allowed_;
};

FakeScreenshotDelegate::FakeScreenshotDelegate(
    std::unique_ptr<UploadJob::ErrorCode> upload_job_error_code,
    bool screenshot_allowed)
    : upload_job_error_code_(std::move(upload_job_error_code)),
      screenshot_allowed_(screenshot_allowed) {}

FakeScreenshotDelegate::~FakeScreenshotDelegate() = default;

bool FakeScreenshotDelegate::IsScreenshotAllowed() {
  return screenshot_allowed_;
}

void FakeScreenshotDelegate::TakeSnapshot(gfx::NativeWindow window,
                                          const gfx::Rect& source_rect,
                                          OnScreenshotTakenCallback callback) {
  EXPECT_TRUE(screenshot_allowed_)
      << "Should not take a screenshot unless it is allowed";

  const int width = source_rect.width();
  const int height = source_rect.height();
  scoped_refptr<base::RefCountedBytes> test_png =
      GenerateTestPNG(width, height);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), test_png));
}

std::unique_ptr<UploadJob> FakeScreenshotDelegate::CreateUploadJob(
    const GURL& upload_url,
    UploadJob::Delegate* delegate) {
  return std::make_unique<MockUploadJob>(upload_url, delegate,
                                         std::move(upload_job_error_code_));
}

}  // namespace

class DeviceCommandScreenshotTest : public ChromeAshTestBase {
 public:
  DeviceCommandScreenshotTest(const DeviceCommandScreenshotTest&) = delete;
  DeviceCommandScreenshotTest& operator=(const DeviceCommandScreenshotTest&) =
      delete;

  void VerifyResults(const RemoteCommandJob& job,
                     RemoteCommandJob::Status expected_status,
                     std::string expected_payload);

 protected:
  DeviceCommandScreenshotTest();

  // ChromeAshTestBase:
  void SetUp() override;

  void InitializeScreenshotJob(RemoteCommandJob* job,
                               RemoteCommandJob::UniqueIDType unique_id,
                               base::TimeTicks issued_time,
                               const std::string& upload_url);

  std::string CreatePayloadFromResultCode(
      DeviceCommandScreenshotJob::ResultCode result_code);

  base::TimeTicks test_start_time_;

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

DeviceCommandScreenshotTest::DeviceCommandScreenshotTest()
    : task_runner_(new base::TestMockTimeTaskRunner()) {}

void DeviceCommandScreenshotTest::SetUp() {
  ChromeAshTestBase::SetUp();
  test_start_time_ = base::TimeTicks::Now();
}

void DeviceCommandScreenshotTest::InitializeScreenshotJob(
    RemoteCommandJob* job,
    RemoteCommandJob::UniqueIDType unique_id,
    base::TimeTicks issued_time,
    const std::string& upload_url) {
  EXPECT_TRUE(job->Init(
      base::TimeTicks::Now(),
      GenerateScreenshotCommandProto(
          unique_id, base::TimeTicks::Now() - issued_time, upload_url),
      em::SignedData()));
  EXPECT_EQ(unique_id, job->unique_id());
  EXPECT_EQ(RemoteCommandJob::NOT_STARTED, job->status());
}

std::string DeviceCommandScreenshotTest::CreatePayloadFromResultCode(
    DeviceCommandScreenshotJob::ResultCode result_code) {
  std::string payload;
  base::Value::Dict root_dict;
  if (result_code != DeviceCommandScreenshotJob::SUCCESS) {
    root_dict.Set(kResultFieldName, result_code);
  }
  base::JSONWriter::Write(root_dict, &payload);
  return payload;
}

void DeviceCommandScreenshotTest::VerifyResults(
    const RemoteCommandJob& job,
    RemoteCommandJob::Status expected_status,
    std::string expected_payload) {
  EXPECT_EQ(expected_status, job.status());
  if (job.status() == RemoteCommandJob::SUCCEEDED) {
    std::unique_ptr<std::string> payload = job.GetResultPayload();
    EXPECT_TRUE(payload);
    EXPECT_EQ(expected_payload, *payload);
  }
}

TEST_F(DeviceCommandScreenshotTest, Success) {
  auto job = std::make_unique<DeviceCommandScreenshotJob>(
      std::make_unique<FakeScreenshotDelegate>(nullptr, true));
  InitializeScreenshotJob(job.get(), kUniqueID, test_start_time_,
                          kMockUploadUrl);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  VerifyResults(
      *job, RemoteCommandJob::SUCCEEDED,
      CreatePayloadFromResultCode(DeviceCommandScreenshotJob::SUCCESS));
}

TEST_F(DeviceCommandScreenshotTest, FailureUserInput) {
  auto job = std::make_unique<DeviceCommandScreenshotJob>(
      std::make_unique<FakeScreenshotDelegate>(nullptr, false));
  InitializeScreenshotJob(job.get(), kUniqueID, test_start_time_,
                          kMockUploadUrl);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  VerifyResults(*job, RemoteCommandJob::FAILED,
                CreatePayloadFromResultCode(
                    DeviceCommandScreenshotJob::FAILURE_USER_INPUT));
}

TEST_F(DeviceCommandScreenshotTest, Failure) {
  using ErrorCode = UploadJob::ErrorCode;
  auto job = std::make_unique<DeviceCommandScreenshotJob>(
      std::make_unique<FakeScreenshotDelegate>(
          std::make_unique<ErrorCode>(UploadJob::AUTHENTICATION_ERROR), true));
  InitializeScreenshotJob(job.get(), kUniqueID, test_start_time_,
                          kMockUploadUrl);
  base::test::TestFuture<void> job_finished_future;
  bool success = job->Run(base::Time::Now(), base::TimeTicks::Now(),
                          job_finished_future.GetCallback());
  EXPECT_TRUE(success);
  ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  VerifyResults(*job, RemoteCommandJob::FAILED,
                CreatePayloadFromResultCode(
                    DeviceCommandScreenshotJob::FAILURE_AUTHENTICATION));
}

}  // namespace policy
