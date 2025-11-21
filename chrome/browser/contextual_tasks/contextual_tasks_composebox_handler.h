// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_

#include "chrome/browser/ui/webui/searchbox/composebox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;

// ComposeboxHandler for the Contextual Tasks UI.
// TODO (crbug.com/458102018): Add separate implementation for SubmitQuery()
// that issues postmessages to the embedded webpage.
class ContextualTasksComposeboxHandler : public ComposeboxHandler {
 public:
  ContextualTasksComposeboxHandler(
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);
  ~ContextualTasksComposeboxHandler() override;

  // Called to update the suggested tab context chip in the compose box.
  virtual void UpdateSuggestedTabContext(searchbox::mojom::TabInfoPtr tab_info);
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
