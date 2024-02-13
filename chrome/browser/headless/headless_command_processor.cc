// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_command_processor.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

namespace headless {

bool ShouldProcessHeadlessCommands() {
  return IsHeadlessMode() && HeadlessCommandHandler::HasHeadlessCommandSwitches(
                                 *base::CommandLine::ForCurrentProcess());
}

void ProcessHeadlessCommands(
    content::BrowserContext* browser_context,
    const GURL& target_url,
    HeadlessCommandHandler::DoneCallback done_callback) {
  DCHECK(browser_context);

  // Ensure lazy loaded content is being captured by the commands.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDisableLazyLoading)) {
    command_line->AppendSwitch(switches::kDisableLazyLoading);
  }

  auto keepalive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::HEADLESS_COMMAND, KeepAliveRestartOption::DISABLED);

  // Create web contents to run the command processing in.
  content::WebContents::CreateParams create_params(browser_context);
  auto web_contents = content::WebContents::Create(create_params);

  // Navigate web contents to the command processor page.
  GURL handler_url = HeadlessCommandHandler::GetHandlerUrl();
  content::NavigationController::LoadURLParams load_url_params(handler_url);
  web_contents->GetController().LoadURLWithParams(load_url_params);

  // Preserve web contents pointer so that the order of ProcessCommands
  // arguments evaluation does not wipe out unique pointer before it's
  // passed to command handler
  content::WebContents* web_contents_ptr = web_contents.get();
  HeadlessCommandHandler::ProcessCommands(
      web_contents_ptr, std::move(target_url),
      base::BindOnce(
          [](std::unique_ptr<content::WebContents> web_contents,
             std::unique_ptr<ScopedKeepAlive> keepalive,
             HeadlessCommandHandler::DoneCallback done_callback,
             HeadlessCommandHandler::Result result) {
            web_contents.reset();
            keepalive.reset();
            std::move(done_callback).Run(result);
          },
          std::move(web_contents), std::move(keepalive),
          std::move(done_callback)));
}

}  // namespace headless
