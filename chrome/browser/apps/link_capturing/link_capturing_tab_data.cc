// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_tab_data.h"

#include <utility>

#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace apps {

namespace {

// WebContentsUserData for storing link capturing data that cannot otherwise be
// inferred during a navigation.
class LinkCapturingUserData
    : public content::WebContentsUserData<LinkCapturingUserData> {
 public:
  ~LinkCapturingUserData() override = default;

  WindowOpenDisposition source_disposition() const {
    return source_disposition_;
  }

  void set_source_disposition(WindowOpenDisposition disposition) {
    source_disposition_ = disposition;
  }

#if BUILDFLAG(IS_CHROMEOS)
  const webapps::AppId& source_app_id() const { return source_app_id_; }

  void set_source_app_id(webapps::AppId app_id) {
    source_app_id_ = std::move(app_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  explicit LinkCapturingUserData(content::WebContents* contents)
      : content::WebContentsUserData<LinkCapturingUserData>(*contents) {}

  friend WebContentsUserData;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // The disposition of the navigation which caused this tab to open.
  WindowOpenDisposition source_disposition_;
#if BUILDFLAG(IS_CHROMEOS)
  // If non-empty, the App ID of the web app where the link that caused this tab
  // to open was clicked.
  webapps::AppId source_app_id_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(LinkCapturingUserData);

}  // namespace

WindowOpenDisposition GetLinkCapturingSourceDisposition(
    content::WebContents* contents) {
  auto* helper = LinkCapturingUserData::FromWebContents(contents);
  if (!helper) {
    return WindowOpenDisposition::UNKNOWN;
  }
  return helper->source_disposition();
}

void SetLinkCapturingSourceDisposition(
    content::WebContents* contents,
    WindowOpenDisposition source_disposition) {
  LinkCapturingUserData::CreateForWebContents(contents);
  auto* helper = LinkCapturingUserData::FromWebContents(contents);
  helper->set_source_disposition(source_disposition);
}

}  // namespace apps
