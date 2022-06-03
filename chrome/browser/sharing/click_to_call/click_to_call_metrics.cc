// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_metrics.h"

#include <cctype>

#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

void LogClickToCallUKM(content::WebContents* web_contents,
                       SharingClickToCallEntryPoint entry_point,
                       bool has_devices,
                       bool has_apps,
                       SharingClickToCallSelection selection) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  if (!ukm_recorder)
    return;

  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  if (source_id == ukm::kInvalidSourceId)
    return;

  ukm::builders::Sharing_ClickToCall(source_id)
      .SetEntryPoint(static_cast<int64_t>(entry_point))
      .SetHasDevices(has_devices)
      .SetHasApps(has_apps)
      .SetSelection(static_cast<int64_t>(selection))
      .Record(ukm_recorder);
}
