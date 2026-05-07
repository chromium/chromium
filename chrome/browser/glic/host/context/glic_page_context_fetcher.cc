// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"

#include <stdint.h>

#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/actor_proto_conversion.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_mojom_traits.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/page_content_annotations/content/page_context_fetcher.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#endif

namespace glic {

class GlicPageContextFetcher {
 public:
  static void LogAnnotatedPageContent(actor::AggregatedJournal* journal,
                                      const GURL& url,
                                      actor::TaskId task_id,
                                      mojo_base::ProtoWrapper& proto_wrapper) {
    if (journal) {
      auto byte_span =
          proto_wrapper.byte_span(mojo_base::ProtoWrapperBytes::GetPassKey());
      if (byte_span.has_value()) {
        journal->LogAnnotatedPageContent(url, task_id, byte_span.value());
      }
    }
  }
};

namespace {

class ChainedProgressListener
    : public page_content_annotations::FetchPageProgressListener {
 public:
  ChainedProgressListener() = default;

  void AddListener(
      std::unique_ptr<page_content_annotations::FetchPageProgressListener>
          listener) {
    listeners_.push_back(std::move(listener));
  }

  void BeginScreenshot() override {
    for (auto& listener : listeners_) {
      listener->BeginScreenshot();
    }
  }

  void ScreenshotCaptured(const SkBitmap& bitmap) override {
    for (auto& listener : listeners_) {
      listener->ScreenshotCaptured(bitmap);
    }
  }

  void ScreenshotRedacted(const SkBitmap& bitmap) override {
    for (auto& listener : listeners_) {
      listener->ScreenshotRedacted(bitmap);
    }
  }

  void EndScreenshot(std::optional<std::string> error) override {
    for (auto& listener : listeners_) {
      listener->EndScreenshot(error);
    }
  }

  void BeginAPC() override {
    for (auto& listener : listeners_) {
      listener->BeginAPC();
    }
  }

  void EndAPC(std::optional<std::string> error) override {
    for (auto& listener : listeners_) {
      listener->EndAPC(error);
    }
  }

 private:
  std::vector<
      std::unique_ptr<page_content_annotations::FetchPageProgressListener>>
      listeners_;
};

void HandleFetchPageResult(
    base::WeakPtr<tabs::TabInterface> tab,
    glic::mojom::TabDataPtr tab_data,
    url::Origin last_committed_origin,
    std::unique_ptr<optimization_guide::proto::ContentNode> media_root_node,
    base::OnceCallback<void(
        base::expected<glic::mojom::GetContextResultPtr,
                       page_content_annotations::FetchPageContextErrorDetails>)>
        callback,
    std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry,
    bool is_screenshot_annotated,
    base::expected<
        std::unique_ptr<page_content_annotations::FetchPageContextResult>,
        page_content_annotations::FetchPageContextErrorDetails> fetch_result) {
  if (!fetch_result.has_value()) {
    std::move(callback).Run(
        base::unexpected(page_content_annotations::FetchPageContextErrorDetails{
            page_content_annotations::FetchPageContextError::kUnknown,
            fetch_result.error().message}));
    return;
  }

  page_content_annotations::FetchPageContextResult& page_context =
      **fetch_result;
  auto tab_context = mojom::TabContext::New();
  tab_context->tab_data = std::move(tab_data);

  if (page_context.inner_text_result) {
    tab_context->web_page_data =
        mojom::WebPageData::New(mojom::DocumentData::New(
            last_committed_origin, page_context.inner_text_result->inner_text,
            page_context.inner_text_result->truncated));
  }

  // TODO(b/446700005): This path is used for both actor and non-actor context
  // fetching, but includes bits specific to actor.
  actor::AggregatedJournal* journal = nullptr;
  actor::TaskId task_id;
  if (journal_entry) {
    journal = &journal_entry->GetJournal();
    task_id = journal_entry->GetTaskId();
  }
  if (page_context.screenshot_result.has_value()) {
    if (journal) {
      journal->LogScreenshot(tab_context->tab_data->url, task_id,
                             page_context.screenshot_result->mime_type,
                             page_context.screenshot_result->screenshot_data);
    }

    tab_context->viewport_screenshot = glic::mojom::Screenshot::New(
        page_context.screenshot_result->dimensions.width(),
        page_context.screenshot_result->dimensions.height(),
        std::move(page_context.screenshot_result->screenshot_data),
        page_context.screenshot_result->mime_type,
        // Implement image annotations (see b/380495633).
        glic::mojom::ImageOriginAnnotations::New());
  }

  if (page_context.pdf_result) {
    // Glic requests PDF bytes exclusively. It does not request PDF text. Only
    // populate `pdf_document_data` if the `pdf_result` holds PDF bytes.
    if (auto* pdf_data =
            std::get_if<std::vector<uint8_t>>(&page_context.pdf_result->data)) {
      auto pdf_document_data = mojom::PdfDocumentData::New();
      pdf_document_data->origin = page_context.pdf_result->origin;
      pdf_document_data->size_limit_exceeded =
          page_context.pdf_result->size_exceeded;
      pdf_document_data->pdf_data = std::move(*pdf_data);
      tab_context->pdf_document_data = std::move(pdf_document_data);
    }
  }

  if (page_context.annotated_page_content_result.has_value()) {
    auto annotated_page_data = mojom::AnnotatedPageData::New();
    if (media_root_node) {
      optimization_guide::proto::ContentNode* media_node =
          page_context.annotated_page_content_result->proto.mutable_root_node()
              ->add_children_nodes();
      media_node->Swap(media_root_node.get());
    }

    if (tab) {
      if (auto* actor_tab_data = actor::ActorTabData::From(tab.get())) {
        actor_tab_data->DidObserveContent(
            page_context.annotated_page_content_result->proto,
            actor::ApcSource::kGlic);
      }
    }

    // If the screenshot is going to be annotated mark the APC as such.
    if (is_screenshot_annotated) {
      page_context.annotated_page_content_result->proto
          .mutable_gemini_in_chrome_page_metadata()
          ->mutable_screenshot_info()
          ->set_has_selection_region_in_screenshot(true);
    }

    annotated_page_data->annotated_page_content = mojo_base::ProtoWrapper(
        page_context.annotated_page_content_result->proto);

    GlicPageContextFetcher::LogAnnotatedPageContent(
        journal, tab_context->tab_data->url, task_id,
        annotated_page_data->annotated_page_content.value());

    annotated_page_data->metadata =
        std::move(page_context.annotated_page_content_result->metadata);

    tab_context->annotated_page_data = std::move(annotated_page_data);
  }
  std::move(callback).Run(
      base::ok(mojom::GetContextResult::NewTabContext(std::move(tab_context))));
}

}  // namespace

void FetchPageContext(
    tabs::TabInterface* tab,
    const mojom::GetTabContextOptions& tab_context_options,
    base::OnceCallback<void(
        base::expected<glic::mojom::GetContextResultPtr,
                       page_content_annotations::FetchPageContextErrorDetails>)>
        callback,
    std::unique_ptr<page_content_annotations::FetchPageProgressListener>
        progress_listener,
    bool is_screenshot_annotated) {
  CHECK(tab);
  CHECK(callback);

  auto* web_contents = tab->GetContents();

  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry;
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  if (auto* actor_keyed_service =
          actor::ActorKeyedService::Get(web_contents->GetBrowserContext())) {
    const GURL& url = web_contents->GetLastCommittedURL();
    journal_entry = actor_keyed_service->GetJournal().CreatePendingAsyncEntry(
        url, actor::TaskId(), actor::kGlobalTrackUUID, "GlicFetchPageContext",
        {});

    auto journal_listener = actor::CreateActorJournalFetchPageProgressListener(
        actor_keyed_service->GetJournal().GetSafeRef(), url, actor::TaskId());
    if (progress_listener) {
      std::unique_ptr<ChainedProgressListener> chained_listener =
          std::make_unique<ChainedProgressListener>();
      chained_listener->AddListener(std::move(journal_listener));
      chained_listener->AddListener(std::move(progress_listener));
      progress_listener = std::move(chained_listener);
    } else {
      progress_listener = std::move(journal_listener);
    }
  }
#endif

  page_content_annotations::FetchPageContextOptions options;
  if (tab_context_options.include_inner_text) {
    options.inner_text_bytes_limit = tab_context_options.inner_text_bytes_limit;
  }
  if (tab_context_options.include_pdf) {
    options.pdf_options.emplace(
        page_content_annotations::PdfOptions::Format::kBytes,
        tab_context_options.pdf_size_limit);
  }

  if (tab_context_options.include_viewport_screenshot) {
    // Disable paint preview backend for glic, and capture the viewport only.
    options.screenshot_options =
        page_content_annotations::ScreenshotOptions::ViewportOnly(
            /*paint_preview_options=*/std::nullopt,
            tab_context_options.screenshot_collection_options);
  }

  const bool on_critical_path = true;
  if (tab_context_options.include_annotated_page_content) {
    if (tab_context_options.annotated_page_content_mode ==
        optimization_guide::proto::
            ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS) {
      options.annotated_page_content_options =
          optimization_guide::ActionableAIPageContentOptions(on_critical_path);
    } else {
      options.annotated_page_content_options =
          optimization_guide::DefaultAIPageContentOptions(on_critical_path);
    }
    options.annotated_page_content_options->max_meta_elements =
        tab_context_options.max_meta_tags;
  }

  std::unique_ptr<optimization_guide::proto::ContentNode> media_root_node;
#if !BUILDFLAG(IS_ANDROID)  // NEEDS_ANDROID_IMPL
  if (auto* media_integration = GlicMediaIntegration::GetFor(web_contents)) {
    media_root_node =
        std::make_unique<optimization_guide::proto::ContentNode>();
    media_integration->AppendContext(web_contents, media_root_node.get());
  }
#endif

  page_content_annotations::FetchPageContext(
      *web_contents, options, std::move(progress_listener),
      base::BindOnce(
          &HandleFetchPageResult, tab->GetWeakPtr(), CreateTabData(tab),
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          std::move(media_root_node), std::move(callback),
          std::move(journal_entry), is_screenshot_annotated));
}

}  // namespace glic
