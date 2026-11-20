// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_pdf_content_analysis.h"

#include <stdint.h>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "components/enterprise/connectors/core/features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace glic {

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
namespace {

bool IsShowingPage(const base::WeakPtr<tabs::TabInterface>& tab,
                   const base::WeakPtr<content::Page>& primary_page) {
  content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
  return web_contents && !web_contents->IsBeingDestroyed() &&
         primary_page.get() == &web_contents->GetPrimaryPage();
}

// Returns the Glic guest WebContents for `tab`, or nullptr if none is open.
content::WebContents* GetGlicGuestWebContents(
    const base::WeakPtr<tabs::TabInterface>& tab) {
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }
  Profile* profile =
      Profile::FromBrowserContext(tab->GetContents()->GetBrowserContext());
  auto* service = GlicKeyedService::Get(profile);
  if (!service) {
    return nullptr;
  }
  GlicInstance* instance = service->GetInstanceForTab(tab.get());
  if (!instance) {
    instance = service->instance_coordinator().GetActiveInstance();
  }
  return instance ? static_cast<GlicInstanceImpl*>(instance)
                        ->host()
                        .web_client_contents()
                  : nullptr;
}

// Returns the settings for scanning a PDF from `tab` with the FILE_ATTACHED
// connector, or nullopt if no scan is needed.
std::optional<enterprise_connectors::ContentAnalysisDelegate::Data>
GetPdfScanData(const base::WeakPtr<tabs::TabInterface>& tab) {
#if BUILDFLAG(IS_ANDROID)
  // OnFileAttachedEnterpriseConnector is marked `future_on` on Android while
  // Clank upload scanning rolls out behind this flag alongside FileSelectHelper
  // and drag-and-drop.
  if (!base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableFileAttachedEnterpriseScanOnClank)) {
    return std::nullopt;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  Profile* profile =
      Profile::FromBrowserContext(tab->GetContents()->GetBrowserContext());
  enterprise_connectors::ContentAnalysisDelegate::Data data;
  if (!enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          profile, GetGuestURL(profile), &data,
          enterprise_connectors::AnalysisConnector::FILE_ATTACHED)) {
    return std::nullopt;
  }
  return data;
}

// Writes `bytes` to a new temporary file. Returns the file's path, or an
// empty path on failure, along with `bytes`.
std::pair<base::FilePath, std::vector<uint8_t>> WritePdfBytesToTempFile(
    std::vector<uint8_t> bytes) {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    return {base::FilePath(), std::move(bytes)};
  }
  if (!base::WriteFile(temp_path, bytes)) {
    base::DeleteFile(temp_path);
    return {base::FilePath(), std::move(bytes)};
  }
  return {std::move(temp_path), std::move(bytes)};
}

// Clears `pdf_result` unless `allowed` is true.
void FinishPdfContentAnalysis(
    std::unique_ptr<page_content_annotations::FetchPageContextResult>
        page_context,
    PdfContentAnalysisCallback callback,
    bool allowed) {
  if (!allowed) {
    page_context->pdf_result.reset();
  }
  std::move(callback).Run(std::move(page_context));
}

void OnPdfContentAnalysisComplete(
    base::OnceCallback<void(bool)> callback,
    const enterprise_connectors::ContentAnalysisDelegate::Data& data,
    enterprise_connectors::ContentAnalysisDelegate::Result& result) {
  std::move(callback).Run(!result.paths_results.empty() &&
                          result.paths_results[0]);
}

// Starts the FILE_ATTACHED scan of the temporary file holding the PDF bytes.
void StartPdfContentAnalysis(
    base::WeakPtr<tabs::TabInterface> tab,
    base::WeakPtr<content::Page> primary_page,
    enterprise_connectors::ContentAnalysisDelegate::Data data,
    std::unique_ptr<page_content_annotations::FetchPageContextResult>
        page_context,
    PdfContentAnalysisCallback callback,
    std::pair<base::FilePath, std::vector<uint8_t>> temp_file_and_bytes) {
  auto [temp_path, bytes] = std::move(temp_file_and_bytes);
  page_context->pdf_result->data = std::move(bytes);
  if (!temp_path.empty()) {
    data.cleanup_closure.ReplaceClosure(base::BindOnce(
        [](base::FilePath path) {
          base::ThreadPool::PostTask(
              FROM_HERE,
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
              base::GetDeleteFileCallback(std::move(path)));
        },
        temp_path));
  }
  content::WebContents* glic_web_contents = GetGlicGuestWebContents(tab);
  // Do not scan, or show the scan dialog, if the tab changed during the write.
  if (temp_path.empty() || !IsShowingPage(tab, primary_page) ||
      !glic_web_contents) {
    page_context->pdf_result.reset();
    std::move(callback).Run(std::move(page_context));
    return;
  }
  data.paths.push_back(std::move(temp_path));
  // The scan drops its callback if the WebContents closes or navigates while
  // the scan dialog is showing, so make sure `callback` still runs, without
  // the PDF. It is posted so that it never runs during WebContents teardown.
  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      glic_web_contents, std::move(data),
      base::BindOnce(&OnPdfContentAnalysisComplete,
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         base::BindPostTaskToCurrentDefault(base::BindOnce(
                             &FinishPdfContentAnalysis, std::move(page_context),
                             std::move(callback))),
                         /*allowed=*/false)),
      enterprise_connectors::DeepScanAccessPoint::UPLOAD);
}

}  // namespace
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)

void MaybeScanPdfForFileAttached(
    base::WeakPtr<tabs::TabInterface> tab,
    base::WeakPtr<content::Page> primary_page,
    std::unique_ptr<page_content_annotations::FetchPageContextResult>
        page_context,
    PdfContentAnalysisCallback callback) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
  // Glic requests PDF bytes exclusively; only those are scanned and shared.
  auto* pdf_bytes =
      page_context->pdf_result
          ? std::get_if<std::vector<uint8_t>>(&page_context->pdf_result->data)
          : nullptr;
  if (pdf_bytes && !pdf_bytes->empty() && IsShowingPage(tab, primary_page)) {
    if (auto data = GetPdfScanData(tab)) {
      // The bytes are scanned from a temporary file, so that what is scanned is
      // exactly what is shared.
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
          base::BindOnce(&WritePdfBytesToTempFile, std::move(*pdf_bytes)),
          base::BindOnce(&StartPdfContentAnalysis, std::move(tab),
                         std::move(primary_page), std::move(*data),
                         std::move(page_context), std::move(callback)));
      return;
    }
  }
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS) || BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::move(page_context));
}

}  // namespace glic
