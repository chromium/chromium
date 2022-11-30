// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_NOTIFICATION_IMAGE_RETAINER_H_
#define CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_NOTIFICATION_IMAGE_RETAINER_H_

#include "chrome/common/notifications/notification_image_retainer.h"

namespace gfx {
class Image;
}  // namespace gfx

// A fake NotificationImageRetainer class for use with unit tests. Returns
// predictable paths to callers wanting to register temporary files.
class FakeNotificationImageRetainer : public NotificationImageRetainer {
 public:
  FakeNotificationImageRetainer() : NotificationImageRetainer() {}
  FakeNotificationImageRetainer(const FakeNotificationImageRetainer&) = delete;
  FakeNotificationImageRetainer& operator=(
      const FakeNotificationImageRetainer&) = delete;
  ~FakeNotificationImageRetainer() override = default;

  // NotificationImageRetainer implementation:
  void CleanupFilesFromPrevSessions() override;
  base::FilePath RegisterTemporaryImage(const gfx::Image& image) override;

 private:
  int counter_ = 0;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WIN_FAKE_NOTIFICATION_IMAGE_RETAINER_H_
