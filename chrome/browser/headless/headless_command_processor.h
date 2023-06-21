// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_COMMAND_PROCESSOR_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_COMMAND_PROCESSOR_H_

#include "base/functional/callback.h"
#include "components/headless/command_handler/headless_command_handler.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace headless {

// Checks if headless command switches --dump-dom, --screenshot or
// --print-to-pdf are present while in headless mode.
bool ShouldProcessHeadlessCommands();

// Runs headless commands against the specified target url.
void ProcessHeadlessCommands(
    content::BrowserContext* browser_context,
    const GURL& target_url,
    HeadlessCommandHandler::DoneCallback done_callback);

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_COMMAND_PROCESSOR_H_
