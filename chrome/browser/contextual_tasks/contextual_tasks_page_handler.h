// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_

#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class ContextualTasksPageHandler : public contextual_tasks::mojom::PageHandler {
 public:
  ContextualTasksPageHandler(
      mojo::PendingRemote<contextual_tasks::mojom::Page> page,
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler);
  ContextualTasksPageHandler(const ContextualTasksPageHandler&) = delete;
  ContextualTasksPageHandler& operator=(const ContextualTasksPageHandler&) =
      delete;
  ~ContextualTasksPageHandler() override;

  // Provides a URL for an AI thread to be loaded as part of the WebUI. A thread
  // is a series of queries and responses with a fixed context.
  void GetThreadUrl(GetThreadUrlCallback callback) override;

 private:
  mojo::Remote<contextual_tasks::mojom::Page> page_;
  mojo::Receiver<contextual_tasks::mojom::PageHandler> page_handler_;
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
