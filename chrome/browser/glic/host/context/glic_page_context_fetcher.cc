// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/media/glic_media_integration.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "url/origin.h"

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
    auto pdf_document_data = mojom::PdfDocumentData::New();
    pdf_document_data->origin = page_context.pdf_result->origin;
    pdf_document_data->size_limit_exceeded =
        page_context.pdf_result->size_exceeded;
    pdf_document_data->pdf_data = std::move(page_context.pdf_result->bytes);
    tab_context->pdf_document_data = std::move(pdf_document_data);
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
            page_context.annotated_page_content_result->proto);
      }
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
        callback) {
  CHECK(tab);
  CHECK(callback);

  auto* web_contents = tab->GetContents();

  std::unique_ptr<actor::AggregatedJournal::PendingAsyncEntry> journal_entry;
  std::unique_ptr<page_content_annotations::FetchPageProgressListener>
      progress_listener;
  if (auto* actor_keyed_service =
          actor::ActorKeyedService::Get(web_contents->GetBrowserContext())) {
    const GURL& url = web_contents->GetLastCommittedURL();
    journal_entry = actor_keyed_service->GetJournal().CreatePendingAsyncEntry(
        url, actor::TaskId(), actor::kGlobalTrackUUID, "GlicFetchPageContext",
        {});
    progress_listener = actor::CreateActorJournalFetchPageProgressListener(
        actor_keyed_service->GetJournal().GetSafeRef(), url, actor::TaskId());
  }

  page_content_annotations::FetchPageContextOptions options;
  if (tab_context_options.include_inner_text) {
    options.inner_text_bytes_limit = tab_context_options.inner_text_bytes_limit;
  }
  if (tab_context_options.include_pdf) {
    options.pdf_size_limit = tab_context_options.pdf_size_limit;
  }

  if (tab_context_options.include_viewport_screenshot) {
    // Disable paint preview backend for glic, and capture the viewport only.
    options.screenshot_options =
        page_content_annotations::ScreenshotOptions::ViewportOnly(
            /*paint_preview_options=*/std::nullopt);
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
  if (auto* media_integration = GlicMediaIntegration::GetFor(web_contents)) {
    media_root_node =
        std::make_unique<optimization_guide::proto::ContentNode>();
    media_integration->AppendContext(web_contents, media_root_node.get());
  }

  page_content_annotations::FetchPageContext(
      *web_contents, options, std::move(progress_listener),
      base::BindOnce(
          &HandleFetchPageResult, tab->GetWeakPtr(),
          CreateTabData(web_contents),
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin(),
          std::move(media_root_node), std::move(callback),
          std::move(journal_entry)));
}

}  // namespace glic
