// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_

#include <memory>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace feedback {

// HelpContentSearchService is responsible for searching for help contents.
class HelpContentSearchService {
 public:
  HelpContentSearchService();
  HelpContentSearchService(const HelpContentSearchService&) = delete;
  HelpContentSearchService& operator=(const HelpContentSearchService&) = delete;
  virtual ~HelpContentSearchService();

  // Populate response with help contents which match the request.
  void virtual Search(const os_feedback_ui::mojom::SearchRequestPtr& request,
                      os_feedback_ui::mojom::SearchResponsePtr& response);
};

// HelpContentProvider is responsible for handling the mojo call for
// GetHelpContents. It delegates the task of finding help contents to a
// HelpContentSearchService. The result is sent back to mojo client.
class HelpContentProvider : os_feedback_ui::mojom::HelpContentProvider {
 public:
  HelpContentProvider();
  HelpContentProvider(std::unique_ptr<HelpContentSearchService> service);
  HelpContentProvider(const HelpContentProvider&) = delete;
  HelpContentProvider& operator=(const HelpContentProvider&) = delete;
  ~HelpContentProvider() override;

  // os_feedback_ui::mojom::HelpContentProvider:
  void GetHelpContents(os_feedback_ui::mojom::SearchRequestPtr request,
                       GetHelpContentsCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::HelpContentProvider>
          receiver);

 private:
  mojo::Receiver<os_feedback_ui::mojom::HelpContentProvider> receiver_{this};
  std::unique_ptr<HelpContentSearchService> help_content_service_;
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HELP_CONTENT_PROVIDER_H_
