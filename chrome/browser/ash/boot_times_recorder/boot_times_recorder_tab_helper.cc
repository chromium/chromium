// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder_tab_helper.h"

#include "chrome/browser/ash/boot_times_recorder/boot_times_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace ash {

// static
void BootTimesRecorderTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  auto* recorder = BootTimesRecorder::GetIfCreated();
  if (!recorder || recorder->is_login_done()) {
    return;
  }
  BootTimesRecorderTabHelper::CreateForWebContents(web_contents);
}

BootTimesRecorderTabHelper::~BootTimesRecorderTabHelper() = default;

void BootTimesRecorderTabHelper::DidStartLoading() {
  BootTimesRecorder::Get()->TabLoadStart(web_contents());
}

void BootTimesRecorderTabHelper::DidStopLoading() {
  BootTimesRecorder::Get()->TabLoadEnd(web_contents());
}

void BootTimesRecorderTabHelper::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  BootTimesRecorder::Get()->RenderFrameHostChanged(old_host, new_host);
}

void BootTimesRecorderTabHelper::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  // This is needed to properly observe events in newly added <webview> tags.
  BootTimesRecorderTabHelper::MaybeCreateForWebContents(inner_web_contents);
}

BootTimesRecorderTabHelper::BootTimesRecorderTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BootTimesRecorderTabHelper>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BootTimesRecorderTabHelper);

}  // namespace ash
