// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/chrome_compose_client.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/compose/core/browser/compose_manager_impl.h"
#include "content/public/browser/web_contents_user_data.h"

ChromeComposeClient::ChromeComposeClient(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromeComposeClient>(*web_contents),
      manager_(this) {}

ChromeComposeClient::~ChromeComposeClient() = default;

void ChromeComposeClient::BindComposeDialog(
    mojo::PendingReceiver<compose::mojom::ComposeDialogPageHandler> handler,
    mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) {
  handler_receiver_ = std::make_unique<
      mojo::Receiver<compose::mojom::ComposeDialogPageHandler>>(
      this, std::move(handler));
  dialog_remote_ =
      std::make_unique<mojo::Remote<compose::mojom::ComposeDialog>>(
          std::move(dialog));
}

void ChromeComposeClient::Compose(compose::mojom::StyleModifiersPtr style,
                                  const std::string& input,
                                  ComposeCallback reply) {
  // TODO(b/302748108) Replace this placeholder code and actually call out to
  // the compose service here.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce([]() {
        // Do some work here.
        return "cucumbers";
      }),
      base::BindOnce(
          [](ComposeCallback callback, const std::string& result) {
            compose::mojom::ComposeResponsePtr response =
                compose::mojom::ComposeResponse::New();
            response->status = compose::mojom::ComposeStatus::kOk;
            response->result = result;
            std::move(callback).Run(std::move(response));
          },
          std::move(reply)));
}

void ChromeComposeClient::ShowComposeDialog(ComposeDialogCallback callback) {
  // TODO(b/301609035) Add the compose dialog call here.
}

compose::ComposeManager& ChromeComposeClient::manager() {
  return manager_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromeComposeClient);
