// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <string>
#include <utility>

#include "chrome/browser/ui/browser_finder.h"

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::ShowComposeDialog(ComposeDialogCallback callback) {
  // TODO(b/301609035) Add the compose dialog call here.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
