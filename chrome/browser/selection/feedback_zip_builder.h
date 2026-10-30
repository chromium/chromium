// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_FEEDBACK_ZIP_BUILDER_H_
#define CHROME_BROWSER_SELECTION_FEEDBACK_ZIP_BUILDER_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace selection {

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

  // Creates a ZIP file at `target_zip_path` (using `base::GetUniquePath` to
  // avoid overwriting an existing file) and passes the created ZIP path to
  // `callback` (or `std::nullopt` on failure).
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
