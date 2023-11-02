// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/win/fake_notification_image_retainer.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/image/image.h"

void FakeNotificationImageRetainer::CleanupFilesFromPrevSessions() {}

base::FilePath FakeNotificationImageRetainer::RegisterTemporaryImage(
    const gfx::Image& image) {
  std::wstring file =
      L"c:\\temp\\img" + base::NumberToWString(counter_++) + L".tmp";
  return base::FilePath(file);
}
