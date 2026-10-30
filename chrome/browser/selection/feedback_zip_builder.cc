// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/feedback_zip_builder.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/selection/suggestion_service.h"
#include "third_party/zlib/google/zip.h"

namespace selection {

struct FeedbackZipBuilder::State {
  SkBitmap screenshot;
  std::optional<EncodedImage> mes_screenshot;
  std::optional<optimization_guide::proto::AnnotatedPageContent> apc;
  std::optional<std::u16string> selected_text;
  std::optional<optimization_guide::proto::SmartSelectionSuggestionsRequest>
      request;
};

FeedbackZipBuilder::FeedbackZipBuilder() : state_(std::make_unique<State>()) {}
FeedbackZipBuilder::FeedbackZipBuilder(FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder& FeedbackZipBuilder::operator=(
    FeedbackZipBuilder&&) noexcept = default;
FeedbackZipBuilder::~FeedbackZipBuilder() = default;

void FeedbackZipBuilder::SetScreenshot(SkBitmap screenshot) {
  CHECK(state_);
  state_->screenshot = std::move(screenshot);
}

void FeedbackZipBuilder::SetMesScreenshot(EncodedImage mes_screenshot) {
  CHECK(state_);
  state_->mes_screenshot = std::move(mes_screenshot);
}

void FeedbackZipBuilder::SetApc(
    optimization_guide::proto::AnnotatedPageContent apc) {
  CHECK(state_);
  state_->apc = std::move(apc);
}

void FeedbackZipBuilder::SetSelectedText(std::u16string selected_text) {
  CHECK(state_);
  state_->selected_text = std::move(selected_text);
}

void FeedbackZipBuilder::SetRequest(
    optimization_guide::proto::SmartSelectionSuggestionsRequest request) {
  CHECK(state_);
  state_->request = std::move(request);
}

void FeedbackZipBuilder::CreateZip(
    base::FilePath target_zip_path,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback) const {
  CHECK(state_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&FeedbackZipBuilder::CreateZipOnThreadPool,
                     std::move(target_zip_path), *state_),
      std::move(callback));
}

// static
std::optional<base::FilePath> FeedbackZipBuilder::CreateZipOnThreadPool(
    base::FilePath target_zip_path,
    State state) {
  if (target_zip_path.empty() ||
      !base::CreateDirectory(target_zip_path.DirName())) {
    return std::nullopt;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    return std::nullopt;
  }

  const base::FilePath dir = temp_dir.GetPath();
  auto get_image_path = [&](std::string_view prefix,
                            const EncodedImage& image) {
    CHECK(image.mime_type == "image/jpeg" || image.mime_type == "image/png");
    std::string_view ext = image.mime_type == "image/jpeg" ? "jpg" : "png";
    return dir.AppendASCII(base::StrCat({prefix, ".", ext}));
  };
  const std::optional<EncodedImage> full_screenshot =
      EncodedImage::Create(state.screenshot);
  if ((full_screenshot.has_value() &&
       !base::WriteFile(get_image_path("screenshot", *full_screenshot),
                        full_screenshot->bytes)) ||
      (state.mes_screenshot.has_value() &&
       !base::WriteFile(get_image_path("mes_screenshot", *state.mes_screenshot),
                        state.mes_screenshot->bytes)) ||
      (state.apc.has_value() &&
       !base::WriteFile(dir.AppendASCII("apc.pb"),
                        state.apc->SerializeAsString())) ||
      (state.selected_text.has_value() &&
       !base::WriteFile(dir.AppendASCII("selection.txt"),
                        base::UTF16ToUTF8(*state.selected_text))) ||
      (state.request.has_value() &&
       !base::WriteFile(dir.AppendASCII("request.pb"),
                        state.request->SerializeAsString()))) {
    return std::nullopt;
  }

  base::FilePath zip_path = base::GetUniquePath(target_zip_path);
  if (zip_path.empty()) {
    return std::nullopt;
  }
  if (!zip::Zip(temp_dir.GetPath(), zip_path, /*include_hidden_files=*/false)) {
    base::DeleteFile(zip_path);
    return std::nullopt;
  }
  return zip_path;
}

}  // namespace selection
