// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/feedback_data.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/buildflags/buildflags.h"

// The feedbackPrivate API is not supported on desktop Android.
static_assert(BUILDFLAG(ENABLE_EXTENSIONS));

namespace extensions {

namespace {

class TestFeedbackService : public FeedbackService {
 public:
  explicit TestFeedbackService(content::BrowserContext* context)
      : FeedbackService(context) {}

  void RedactThenSendFeedback(const FeedbackParams& params,
                              scoped_refptr<feedback::FeedbackData> data,
                              FeedbackAttachments attachments,
                              SendFeedbackCallback callback) override {
    ++request_count_;
    const bool has_file =
        data->description() == "attachment" || data->description() == "both";
    const bool has_screenshot =
        data->description() == "screenshot" || data->description() == "both";
    EXPECT_EQ(has_file, attachments.attached_file.is_valid());
    EXPECT_EQ(has_screenshot, attachments.screenshot.is_valid());
    base::RepeatingClosure done =
        base::BarrierClosure(attachments.attached_file.is_valid() +
                                 attachments.screenshot.is_valid(),
                             base::BindOnce(std::move(callback), true));
    if (attachments.attached_file) {
      EXPECT_EQ("attachment.txt", data->attached_filename());
      BlobReader::Read(
          std::move(attachments.attached_file),
          base::BindOnce(CheckBlobContents, "attached file contents")
              .Then(done));
    }
    if (attachments.screenshot) {
      BlobReader::Read(
          std::move(attachments.screenshot),
          base::BindOnce(CheckBlobContents, "screenshot contents").Then(done));
    }
  }

  int request_count() const { return request_count_; }

 private:
  ~TestFeedbackService() override = default;

  static void CheckBlobContents(const std::string& expected,
                                std::string actual,
                                int64_t length) {
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(expected.size(), length);
  }

  int request_count_ = 0;
};

}  // namespace

using FeedbackPrivateApiTest = ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(FeedbackPrivateApiTest, Basic) {
  scoped_refptr<TestFeedbackService> service =
      base::MakeRefCounted<TestFeedbackService>(profile());
  FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(profile())
      ->SetFeedbackServiceForTesting(service);
  EXPECT_TRUE(RunExtensionTest("feedback_private/basic")) << message_;
  EXPECT_EQ(4, service->request_count());
}

}  // namespace extensions
