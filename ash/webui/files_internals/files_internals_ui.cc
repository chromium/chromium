// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/files_internals/files_internals_ui.h"

#include "ash/webui/files_internals/url_constants.h"
#include "ash/webui/grit/ash_files_internals_resources.h"
#include "ash/webui/grit/ash_files_internals_resources_map.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "content/public/browser/web_contents.h"

namespace ash {

namespace {
const char kGetFileTasksHtmlQuestion[] = "getFileTasks.html?";
}  // namespace

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
  CallSetRequestFilter(data_source);
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

void FilesInternalsUI::CallSetRequestFilter(
    content::WebUIDataSource* data_source) {
  auto should_handle_request_callback =
      base::BindRepeating([](const std::string& url_path_query) -> bool {
        return (url_path_query == "debug.json") ||
               (url_path_query == "downloads_fsurls.html") ||
               base::StartsWith(url_path_query, kGetFileTasksHtmlQuestion);
      });

  auto handle_request_callback = base::BindRepeating(
      &FilesInternalsUI::HandleRequest, weak_ptr_factory_.GetWeakPtr());

  data_source->SetRequestFilter(std::move(should_handle_request_callback),
                                std::move(handle_request_callback));
}

void FilesInternalsUI::HandleRequest(
    const std::string& url_path_query,
    content::WebUIDataSource::GotDataCallback callback) {
  if (url_path_query == "debug.json") {
    delegate_->GetDebugJSON(base::BindOnce(
        [](content::WebUIDataSource::GotDataCallback callback,
           const base::Value& value) {
          std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
              value.DebugString()));
        },
        std::move(callback)));
    return;
  }

  base::OnceCallback<void(const std::string_view)> string_callback =
      base::BindOnce(
          [](content::WebUIDataSource::GotDataCallback callback,
             std::string_view value) {
            std::move(callback).Run(
                base::MakeRefCounted<base::RefCountedString>(
                    std::string(value)));
          },
          std::move(callback));

  if (url_path_query == "downloads_fsurls.html") {
    delegate_->GetDownloadsFSURLs(std::move(string_callback));
    return;
  }

  if (base::StartsWith(url_path_query, kGetFileTasksHtmlQuestion)) {
    std::string file_system_url;

    base::StringPairs params;
    if (base::SplitStringIntoKeyValuePairs(
            url_path_query.substr(strlen(kGetFileTasksHtmlQuestion)), '=', '&',
            &params)) {
      for (const auto& param : params) {
        if (param.first == "fsurl") {
          file_system_url = base::UnescapeBinaryURLComponent(param.second);
        }
      }
    }

    delegate_->GetFileTasks(file_system_url, std::move(string_callback));
    return;
  }

  NOTREACHED();
}

WEB_UI_CONTROLLER_TYPE_IMPL(FilesInternalsUI)

}  // namespace ash
