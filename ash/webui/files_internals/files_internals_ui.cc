// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/files_internals/files_internals_ui.h"

#include "ash/webui/files_internals/url_constants.h"
#include "ash/webui/grit/ash_files_internals_resources.h"
#include "ash/webui/grit/ash_files_internals_resources_map.h"
#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/web_contents.h"

namespace ash {

FilesInternalsUI::FilesInternalsUI(
    content::WebUI* web_ui,
    std::unique_ptr<FilesInternalsUIDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  content::WebUIDataSource* data_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          kChromeUIFilesInternalsHost);
  data_source->AddResourcePath("", IDR_ASH_FILES_INTERNALS_INDEX_HTML);
  data_source->AddResourcePaths(base::make_span(
      kAshFilesInternalsResources, kAshFilesInternalsResourcesSize));
  SetRequestFilterDebugJson(data_source);
}

FilesInternalsUI::~FilesInternalsUI() = default;

void FilesInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::files_internals::PageHandler> receiver) {
  page_handler_ =
      std::make_unique<FilesInternalsPageHandler>(this, std::move(receiver));
}

FilesInternalsUIDelegate* FilesInternalsUI::delegate() {
  return delegate_.get();
}

void FilesInternalsUI::SetRequestFilterDebugJson(
    content::WebUIDataSource* data_source) {
  auto should_handle_request_callback = base::BindRepeating(
      [](const std::string& url) -> bool { return url == "debug.json"; });

  auto handle_request_callback =
      base::BindRepeating(&FilesInternalsUI::HandleRequestDebugJson,
                          weak_ptr_factory_.GetWeakPtr());

  data_source->SetRequestFilter(std::move(should_handle_request_callback),
                                std::move(handle_request_callback));
}

void FilesInternalsUI::HandleRequestDebugJson(
    const std::string& url,
    content::WebUIDataSource::GotDataCallback callback) {
  delegate_->GetDebugJSON(base::BindOnce(
      [](content::WebUIDataSource::GotDataCallback callback,
         const base::Value& value) {
        std::move(callback).Run(
            base::MakeRefCounted<base::RefCountedString>(value.DebugString()));
      },
      std::move(callback)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(FilesInternalsUI)

}  // namespace ash
