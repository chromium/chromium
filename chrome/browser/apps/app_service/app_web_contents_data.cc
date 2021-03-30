// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_web_contents_data.h"

namespace apps {

AppWebContentsData::AppWebContentsData(content::WebContents* web_contents,
                                       Client* client)
    : content::WebContentsObserver(web_contents), client_(client) {
  DCHECK(web_contents);
  DCHECK(client);
}

void AppWebContentsData::WebContentsDestroyed() {
  if (client_) {
    client_->OnWebContentsDestroyed(web_contents());
  }
  client_ = nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppWebContentsData)

}  // namespace apps
