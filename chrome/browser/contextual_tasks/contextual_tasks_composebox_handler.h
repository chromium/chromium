// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

class Profile;
class ContextualTasksUI;

// ComposeboxHandler for the Contextual Tasks UI.
class ContextualTasksComposeboxHandler : public ComposeboxHandler {
 public:
  ContextualTasksComposeboxHandler(
      ContextualTasksUI* ui_controller,
      Profile* profile,
      content::WebContents* web_contents,
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler);
  ~ContextualTasksComposeboxHandler() override;

  // composebox::mojom::PageHandler:
  void SubmitQuery(const std::string& query_text,
                   uint8_t mouse_button,
                   bool alt_key,
                   bool ctrl_key,
                   bool meta_key,
                   bool shift_key) override;
  void HandleLensButtonClick() override;
  void DeleteContext(const base::UnguessableToken& file_token,
                     bool from_automatic_chip) override;

  void CreateAndSendQueryMessage(const std::string& query);

  OmniboxController* GetOmniboxControllerForTesting() {
    return omnibox_controller();
  }

 private:

  raw_ptr<ContextualTasksUI> web_ui_controller_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_COMPOSEBOX_HANDLER_H_
