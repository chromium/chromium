// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_storage.h"

namespace privacy_sandbox {

MockNoticeStorage::MockNoticeStorage() = default;
MockNoticeStorage::~MockNoticeStorage() = default;

void MockNoticeStorage::RecordEvent(
    std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id,
    notice::mojom::PrivacySandboxNoticeEvent event) {
  RecordEventWithNoticeId(notice_id, event);
}

}  // namespace privacy_sandbox
