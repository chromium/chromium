// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/mock_notification_image_retainer.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/image/image.h"

void MockNotificationImageRetainer::CleanupFilesFromPrevSessions() {}

base::FilePath MockNotificationImageRetainer::RegisterTemporaryImage(
    const gfx::Image& image) {
  base::string16 file = base::string16(L"c:\\temp\\img") +
                        base::IntToString16(counter_++) +
                        base::string16(L".tmp");
  return base::FilePath(file);
}
