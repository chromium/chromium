// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_
#define CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "components/paint_preview/browser/paint_preview_base_service.h"
#include "components/paint_preview/browser/paint_preview_policy.h"

namespace content {
class WebContents;
}  // namespace content

namespace long_screenshots {

// A service for capturing Long Screenshots using PaintPreview.
class LongScreenshotsTabService
    : public paint_preview::PaintPreviewBaseService {
 public:
  LongScreenshotsTabService(
      const base::FilePath& profile_dir,
      base::StringPiece ascii_feature_name,
      std::unique_ptr<paint_preview::PaintPreviewPolicy> policy,
      bool is_off_the_record);
  ~LongScreenshotsTabService() override;

  enum Status {
    kOk = 0,
    kDirectoryCreationFailed = 1,
    kCaptureFailed = 2,
    kProtoSerializationFailed = 3,
    kWebContentsGone = 4,
  };

  using FinishedCallback = base::OnceCallback<void(Status)>;

  // Captures a Paint Preview of |contents| which should be associated with
  // |tab_id| for storage. |callback| is invoked on completion to indicate
  // status.
  void CaptureTab(int tab_id,
                  content::WebContents* contents,
                  FinishedCallback callback);

 private:
  // The FTN ID is to look-up the content::WebContents.
  void CaptureTabInternal(int tab_id,
                          const paint_preview::DirectoryKey& key,
                          int frame_tree_node_id,
                          content::GlobalFrameRoutingId frame_routing_id,
                          FinishedCallback callback,
                          const base::Optional<base::FilePath>& file_path);

  void OnCaptured(int tab_id,
                  const paint_preview::DirectoryKey& key,
                  int frame_tree_node_id,
                  FinishedCallback callback,
                  paint_preview::PaintPreviewBaseService::CaptureStatus status,
                  std::unique_ptr<paint_preview::CaptureResult> result);

  void OnFinished(int tab_id, FinishedCallback callback, bool success);

  base::WeakPtrFactory<LongScreenshotsTabService> weak_ptr_factory_{this};
};

}  // namespace long_screenshots

#endif  // CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_H_
