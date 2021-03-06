// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_H_
#define CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/lite_video/lite_video_decider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace lite_video {
class LiteVideoDecider;
}  // namespace lite_video

// Keyed service than can be used to receive requests for enabling LiteVideos on
// current navigations.
class LiteVideoKeyedService : public KeyedService {
 public:
  explicit LiteVideoKeyedService(content::BrowserContext* browser_context);
  ~LiteVideoKeyedService() override;

  // Initializes the service. |profile_path| is the path to user data on disk.
  void Initialize(const base::FilePath& profile_path);

  lite_video::LiteVideoDecider* lite_video_decider() { return decider_.get(); }

 private:
  friend class ChromeBrowsingDataRemoverDelegate;

  // Clears data specific to the user between the provided times.
  void ClearData(const base::Time& delete_begin, const base::Time& delete_end);

  // The decider owned by this keyed service capable of determining whether
  // to apply the LiteVideo optimization to a navigation.
  std::unique_ptr<lite_video::LiteVideoDecider> decider_;

  // Guaranteed to outlive |this|.
  content::BrowserContext* browser_context_;
};

#endif  // CHROME_BROWSER_LITE_VIDEO_LITE_VIDEO_KEYED_SERVICE_H_
