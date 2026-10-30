// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_FEEDBACK_ZIP_BUILDER_H_
#define CHROME_BROWSER_SELECTION_FEEDBACK_ZIP_BUILDER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace selection {

struct EncodedImage;

// Collects artifacts from a Smart Selection request and packages them into a
// dogfood feedback ZIP archive on demand.
class FeedbackZipBuilder {
 public:
  FeedbackZipBuilder();
  FeedbackZipBuilder(const FeedbackZipBuilder&) = delete;
  FeedbackZipBuilder& operator=(const FeedbackZipBuilder&) = delete;
  FeedbackZipBuilder(FeedbackZipBuilder&&) noexcept;
  FeedbackZipBuilder& operator=(FeedbackZipBuilder&&) noexcept;
  ~FeedbackZipBuilder();

  void SetScreenshot(SkBitmap screenshot);
  void SetMesScreenshot(EncodedImage mes_screenshot);
  void SetApc(optimization_guide::proto::AnnotatedPageContent apc);
  void SetSelectedText(std::u16string selected_text);
  void SetRequest(
      optimization_guide::proto::SmartSelectionSuggestionsRequest request);

  // Creates a ZIP file at `target_zip_path` (using `base::GetUniquePath` to
  // avoid overwriting an existing file) containing the artifacts recorded
  // from the `RequestSuggestions()` call:
  // - `screenshot.{png,jpg}`: the full uncropped screenshot,
  // - `mes_screenshot.{png,jpg}`: the cropped/downscaled screenshot sent to
  //   MES,
  // - `apc.pb`: the serialized AnnotatedPageContent proto,
  // - `selection.txt`: the UTF-8 text selection,
  // - `request.pb`: the serialized SmartSelectionSuggestionsRequest proto,
  // and passes the created ZIP path to `callback` (or `std::nullopt` on
  // failure).
  void CreateZip(
      base::FilePath target_zip_path,
      base::OnceCallback<void(std::optional<base::FilePath>)> callback) const;

 private:
  struct State;

  static std::optional<base::FilePath> CreateZipOnThreadPool(
      base::FilePath target_zip_path,
      State state);

  std::unique_ptr<State> state_;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_FEEDBACK_ZIP_BUILDER_H_
