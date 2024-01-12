// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/fake_screenshot_delegate.h"

#include <string>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/uploading/upload_job.h"

namespace policy {

namespace {
class FakeUploadJob : public policy::UploadJob {
 public:
  explicit FakeUploadJob(UploadJob::Delegate* delegate) : delegate_(delegate) {
    DCHECK(delegate_);
  }

  ~FakeUploadJob() override = default;
  FakeUploadJob(const FakeUploadJob&) = delete;
  FakeUploadJob& operator=(const FakeUploadJob&) = delete;

  void AddDataSegment(const std::string& name,
                      const std::string& filename,
                      const std::map<std::string, std::string>& header_entries,
                      std::unique_ptr<std::string> data) override {
    // ignore data segments
    return;
  }

  void Start() override {
    // just call OnSuccess to complete a remote command
    delegate_->OnSuccess();
  }

 private:
  raw_ptr<UploadJob::Delegate> delegate_;
};
}  // namespace

bool FakeScreenshotDelegate::IsScreenshotAllowed() {
  return true;
}

void FakeScreenshotDelegate::TakeSnapshot(
    gfx::NativeWindow window,
    const gfx::Rect& source_rect,
    policy::OnScreenshotTakenCallback callback) {
  std::move(callback).Run(nullptr);
}

std::unique_ptr<policy::UploadJob> FakeScreenshotDelegate::CreateUploadJob(
    const GURL&,
    policy::UploadJob::Delegate* delegate) {
  return std::make_unique<FakeUploadJob>(delegate);
}

}  // namespace policy
