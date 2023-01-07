// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/files_internals/files_internals.h"

#include "ash/webui/files_internals/url_constants.h"
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

  auto should_handle_request_callback =
      base::BindRepeating([](const std::string&) -> bool { return true; });

  auto handle_request_callback = base::BindRepeating(
      &FilesInternalsUI::HandleRequest, weak_ptr_factory_.GetWeakPtr());

  data_source->SetRequestFilter(std::move(should_handle_request_callback),
                                std::move(handle_request_callback));
}

FilesInternalsUI::~FilesInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(FilesInternalsUI)

void FilesInternalsUI::HandleRequest(
    const std::string& url,
    content::WebUIDataSource::GotDataCallback callback) {
  // The content type is derived from the string url, so redirect an empty url
  // to "debug.json".
  if (url.empty()) {
    std::string s(
        "<html><head><meta "
        "http-equiv=refresh content=\"0; url='debug.json'\"/></head></html>");
    std::move(callback).Run(base::RefCountedString::TakeString(&s));
    return;
  }

  std::string s = delegate_->GetDebugJSON().DebugString();
  std::move(callback).Run(base::RefCountedString::TakeString(&s));
}

}  // namespace ash
