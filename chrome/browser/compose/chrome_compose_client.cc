// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/browser_finder.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "content/public/browser/web_contents_user_data.h"

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      compose_manager_(/*client=*/this) {}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::ShowComposeDialog(ComposeDialogCallback callback) {
  // TODO(b/301609035) Add the compose dialog call here.
}

compose::ComposeManager& ChromeComposeClient::manager() {
  return compose_manager_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
