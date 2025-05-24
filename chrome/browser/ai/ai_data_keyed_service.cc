// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/barrier_callback.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/functional/concurrent_closures.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "base/unguessable_token.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/form_processing/optimization_guide_proto_util.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/compose/buildflags.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "pdf/buildflags.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tabs/public/tab_group.h"
#endif

#if BUILDFLAG(ENABLE_PDF)
// GN doesn't understand buildflags, erroring on Android builds
#include "components/pdf/browser/pdf_document_helper.h"  // nogncheck
#include "components/pdf/common/constants.h"             // nogncheck
#endif  // BUILDFLAG(ENABLE_PDF)

namespace {

#if BUILDFLAG(ENABLE_PDF)
constexpr size_t kBytesPerMegabyte = 1'000'000;
constexpr size_t kPdfUploadLimitBytes = 128 * kBytesPerMegabyte;
#endif  // BUILDFLAG(ENABLE_PDF)

void OnGotAIPageContentForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    std::optional<optimization_guide::AIPageContentResult> page_content) {
  TRACE_EVENT("browser", "OnGotAIPageContentForModelPrototyping");

  AiDataKeyedService::BrowserData data;
  if (page_content) {
    *data.mutable_page_context()->mutable_annotated_page_content() =
        std::move(page_content->proto);
    std::move(continue_callback).Run(std::move(data));
    return;
  }

  std::move(continue_callback).Run(std::nullopt);
}

void OnGotAIPageContentWithActionableElementsForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    std::optional<optimization_guide::AIPageContentResult> page_content) {
  TRACE_EVENT("browser",
              "OnGotAIPageContentWithActionableElementsForModelPrototyping");

  AiDataKeyedService::BrowserData data;
  if (page_content) {
    *data.mutable_action_annotated_page_content() =
        std::move(page_content->proto);
    std::move(continue_callback).Run(std::move(data));
    return;
  }

  std::move(continue_callback).Run(std::nullopt);
}

void GetAIPageContentForModelPrototyping(
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT("browser", "GetAIPageContentForModelPrototyping");

  auto options = optimization_guide::DefaultAIPageContentOptions();
  options->enable_experimental_actionable_data = false;
  options->include_geometry = false;
  optimization_guide::OnAIPageContentDone callback = base::BindOnce(
      &OnGotAIPageContentForModelPrototyping, std::move(continue_callback));
  optimization_guide::GetAIPageContent(web_contents, std::move(options),
                                       std::move(callback));
}

void GetAIPageContentWithActionableElementsForModelPrototyping(
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT("browser",
              "GetAIPageContentWithActionableElementsForModelPrototyping");

  auto options = optimization_guide::DefaultAIPageContentOptions();
  options->enable_experimental_actionable_data = true;
  options->include_geometry = true;
  optimization_guide::OnAIPageContentDone callback = base::BindOnce(
      &OnGotAIPageContentWithActionableElementsForModelPrototyping,
      std::move(continue_callback));
  optimization_guide::GetAIPageContent(web_contents, std::move(options),
                                       std::move(callback));
}

// Fills an AiData proto with information from GetInnerText. If no result,
// returns an empty AiDAta.
void OnGetInnerTextForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  TRACE_EVENT0("browser", "OnGetInnerTextForModelPrototyping");
  AiDataKeyedService::AiData data;
  if (result) {
    data = std::make_optional<AiDataKeyedService::BrowserData>();
    data->mutable_page_context()->set_inner_text(result->inner_text);
    if (result->node_offset) {
      data->mutable_page_context()->set_inner_text_offset(
          result->node_offset.value());
    }
  }
  std::move(continue_callback).Run(std::move(data));
}

// Calls GetInnerText and creates a WrapCallbackWithDefaultInvokeIfNotRun with
// nullptr.
void GetInnerTextForModelPrototyping(
    int dom_node_id,
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT0("browser", "GetInnerTextForModelPrototyping");
  DCHECK(web_contents);
  // If the tab has not actually navigated, then the remote interfaces will be
  // null, just leave off inner text in this case.
  if (!web_contents->GetPrimaryMainFrame() ||
      !web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()) {
    return std::move(continue_callback)
        .Run(std::make_optional<AiDataKeyedService::BrowserData>());
  }
  content_extraction::GetInnerText(
      *web_contents->GetPrimaryMainFrame(), dom_node_id,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnGetInnerTextForModelPrototyping,
                         std::move(continue_callback)),
          nullptr));
}

void OnHistorySearchCompleted(
    AiDataKeyedService::AiDataCallback ai_data_callback,
    std::vector<history_embeddings::SearchResult> search_results) {
  AiDataKeyedService::AiData data =
      std::make_optional<AiDataKeyedService::BrowserData>();
  for (const auto& search_result : search_results) {
    // Skip search result if empty;
    if (search_result.scored_url_rows.empty()) {
      continue;
    }

    auto* history_result = data->add_history_query_result();
    auto* query = history_result->mutable_query();
    query->set_query(search_result.query);
    query->set_num_history_visits(search_result.count);
    if (search_result.time_range_start) {
      query->mutable_history_search_time_range()
          ->mutable_start_time()
          ->set_seconds((int64_t)(search_result.time_range_start
                                      ->InSecondsFSinceUnixEpoch()));
    }
    for (auto& scored_url_row : search_result.scored_url_rows) {
      optimization_guide::proto::HistoryVisitItem* visit_item =
          history_result->mutable_history_data()->add_visit_item();
      visit_item->set_page_title(base::UTF16ToUTF8(scored_url_row.row.title()));
      visit_item->set_page_url(scored_url_row.row.url().spec());
      visit_item->mutable_visit_time()->set_seconds(static_cast<int64_t>(
          scored_url_row.scored_url.visit_time.InSecondsFSinceUnixEpoch()));
      for (const std::string& passage :
           scored_url_row.passages_embeddings.passages.passages()) {
        visit_item->add_passages(passage);
      }
    }
  }

  std::move(ai_data_callback).Run(data);
}

void GetHistoryQueryResultForModelPrototyping(
    content::WebContents* web_contents,
    const optimization_guide::proto::HistoryQuerySpecifiers& history_specifiers,
    AiDataKeyedService::AiDataCallback continue_callback) {
  history_embeddings::HistoryEmbeddingsService* history_embeddings_service =
      HistoryEmbeddingsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));

  const auto history_search_callback =
      base::BarrierCallback<history_embeddings::SearchResult>(
          history_specifiers.history_queries_size(),
          base::BindOnce(&OnHistorySearchCompleted,
                         std::move(continue_callback)));

  for (const auto& history_query : history_specifiers.history_queries()) {
    // Skip empty queries by returning an empty result.
    if (history_query.query().empty()) {
      history_search_callback.Run(history_embeddings::SearchResult());
      continue;
    }

    history_embeddings_service->Search(
        /*previous_search_result=*/nullptr, history_query.query(),
        base::Time::FromSecondsSinceUnixEpoch(
            (double)(history_query.history_search_time_range()
                         .start_time()
                         .seconds())),
        history_query.num_history_visits(),
        /*skip_answering=*/true, history_search_callback);
  }
}

// Fills an AiData proto with information from RequestAXTreeSnapshot. If no
// result, returns an empty AiData.
void OnRequestAxTreeSnapshotForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    ui::AXTreeUpdate& ax_tree_update) {
  TRACE_EVENT0("browser", "OnRequestAxTreeSnapshotForModelPrototyping");
  AiDataKeyedService::AiData data;
  if (ax_tree_update.has_tree_data) {
    data = std::make_optional<AiDataKeyedService::BrowserData>();
    optimization_guide::PopulateAXTreeUpdateProto(
        ax_tree_update, data->mutable_page_context()->mutable_ax_tree_data());
  }

  std::move(continue_callback).Run(std::move(data));
}

// Calls RequestAXTreeSnapshot and creates a
// WrapCallbackWithDefaultInvokeIfNotRun with an empty AxTreeUpdate.
void RequestAxTreeSnapshotForModelPrototyping(
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT0("browser", "RequestAxTreeSnapshotForModelPrototyping");
  DCHECK(web_contents);
  ui::AXTreeUpdate update;
  web_contents->RequestAXTreeSnapshot(
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnRequestAxTreeSnapshotForModelPrototyping,
                         std::move(continue_callback)),
          base::OwnedRef(std::move(update))),
      ui::kAXModeWebContentsOnly, 50000,
      /*timeout=*/{},
      content::WebContents::AXTreeSnapshotPolicy::kSameOriginDirectDescendants);
}

#if BUILDFLAG(ENABLE_PDF)
// Returns the PDFHelper associated with the given web contents. Returns nullptr
// if one does not exist.
pdf::PDFDocumentHelper* MaybeGetFullPagePdfHelper(
    content::WebContents* contents) {
  // MIME type associated with `contents` must be `application/pdf` for a
  // full-page PDF.
  if (contents->GetContentsMimeType() != pdf::kPDFMimeType) {
    return nullptr;
  }

  return pdf::PDFDocumentHelper::MaybeGetForWebContents(contents);
}

void OnRequestPdfBytesForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  TRACE_EVENT0("browser", "OnRequestPdfBytesForModelPrototyping");

  auto data = std::make_optional<AiDataKeyedService::BrowserData>();
  if (status != pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess ||
      bytes.empty()) {
    std::move(continue_callback).Run(std::move(data));
    return;
  }

  data->mutable_page_context()->set_pdf_data(base::Base64Encode(bytes));
  std::move(continue_callback).Run(std::move(data));
}

void RequestPdfBytesForModelPrototyping(
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT0("browser", "RequestPdfBytesForModelPrototyping");
  DCHECK(web_contents);

  pdf::PDFDocumentHelper* pdf_helper = MaybeGetFullPagePdfHelper(web_contents);
  if (!pdf_helper) {
    std::move(continue_callback)
        .Run(std::make_optional<AiDataKeyedService::BrowserData>());
    return;
  }

  pdf_helper->GetPdfBytes(
      kPdfUploadLimitBytes,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnRequestPdfBytesForModelPrototyping,
                         std::move(continue_callback)),
          pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed,
          std::vector<uint8_t>(), 0));
}
#endif  // BUILDFLAG(ENABLE_PDF)

// Once all callbacks are run, merges the AiDatas and returns the filled AiData.
// If any did not complete, returns an empty AiData.
void OnDataCollectionsComplete(AiDataKeyedService::AiDataCallback callback,
                               AiDataKeyedService::AiData data,
                               std::vector<AiDataKeyedService::AiData> datas) {
  TRACE_EVENT0("browser", "OnDataCollectionsComplete");
  DCHECK(data);
  for (const auto& data_slice : datas) {
    if (!data_slice) {
      // Return an empty data to indicate an error.
      return std::move(callback).Run(data_slice);
    }
    data->MergeFrom(data_slice.value());
  }
  std::move(callback).Run(std::move(data));
}

#if !BUILDFLAG(IS_ANDROID)
void OnGetTabInnerText(
    int64_t tab_id,
    std::string title,
    std::string url,
    AiDataKeyedService::AiDataCallback continue_callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  TRACE_EVENT0("browser", "OnGetTabInnerText");
  auto data = std::make_optional<AiDataKeyedService::BrowserData>();
  auto* tab = data->add_tabs();
  tab->set_tab_id(tab_id);
  tab->set_title(std::move(title));
  tab->set_url(std::move(url));
  if (result) {
    tab->mutable_page_context()->set_inner_text(result->inner_text);
  }
  std::move(continue_callback).Run(std::move(data));
}

// Gets tab info and starts a call to get the inner text from the tab.
void FillTabInfo(content::WebContents* web_contents,
                 AiDataKeyedService::AiDataCallback continue_callback,
                 int64_t tab_id,
                 std::string title,
                 std::string url) {
  TRACE_EVENT0("browser", "FillTabInfo");
  DCHECK(web_contents);
  // If the tab has not actually navigated, then the remote interfaces will be
  // null, just leave off inner text in this case.
  if (!web_contents->GetPrimaryMainFrame() ||
      !web_contents->GetPrimaryMainFrame()->GetRemoteInterfaces()) {
    return std::move(continue_callback)
        .Run(std::make_optional<AiDataKeyedService::BrowserData>());
  }
  content_extraction::GetInnerText(
      *web_contents->GetPrimaryMainFrame(), std::nullopt,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnGetTabInnerText, tab_id, std::move(title),
                         std::move(url), std::move(continue_callback)),
          nullptr));
}

// Create an AiData with the tab and tab group information.
void GetTabDataForModelPrototyping(
    int tabs_for_inner_text,
    content::WebContents* web_contents,
    base::ConcurrentCallbacks<AiDataKeyedService::AiData>& concurrent) {
  TRACE_EVENT0("browser", "GetTabDataForModelPrototyping");
  // Get the browser window that contains the web contents the extension is
  // being targeted on. If there isn't a window, or there isn't a tab strip
  // model, return an empty AiData.
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser || !browser->GetTabStripModel()) {
    return concurrent.CreateCallback().Run(std::nullopt);
  }

  // Fill the Tabs part of the proto.
  AiDataKeyedService::AiData data =
      std::make_optional<AiDataKeyedService::BrowserData>();
  auto* tab_strip_model = browser->GetTabStripModel();
  for (int index = 0; index < tab_strip_model->count(); index++) {
    content::WebContents* tab_web_contents =
        tab_strip_model->GetWebContentsAt(index);
    auto title = base::UTF16ToUTF8(tab_web_contents->GetTitle());
    auto url = tab_web_contents->GetLastCommittedURL().spec();
    if (index >= tabs_for_inner_text) {
      OnGetTabInnerText(index, std::move(title), std::move(url),
                        concurrent.CreateCallback(), nullptr);
    } else {
      FillTabInfo(tab_web_contents, concurrent.CreateCallback(), index,
                  std::move(title), std::move(url));
    }
    if (web_contents == tab_web_contents) {
      data->set_active_tab_id(index);
    }
  }

  // Fill the Tab Groups part of the proto.
  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  for (tab_groups::TabGroupId group_id : tab_group_model->ListTabGroups()) {
    TabGroup* group = tab_group_model->GetTabGroup(group_id);
    auto* group_data = data->add_pre_existing_tab_groups();
    group_data->set_group_id(group_id.ToString());
    group_data->set_label(base::UTF16ToUTF8(group->visual_data()->title()));

    const gfx::Range tab_indices = group->ListTabs();
    for (size_t index = tab_indices.start(); index < tab_indices.end();
         index++) {
      group_data->add_tabs()->set_tab_id(index);
    }
  }
  concurrent.CreateCallback().Run(std::move(data));
}

void GetFormDataByFieldGlobalIdForModelPrototyping(
    content::WebContents* web_contents,
    const optimization_guide::proto::AutofillFieldGlobalId& global_id_proto,
    AiDataKeyedService::AiDataCallback continue_callback) {
  AiDataKeyedService::AiData data = AiDataKeyedService::BrowserData();

  // Construct an `autofill::FieldGlobalId` from `global_id_proto`.
  std::optional<base::UnguessableToken> frame_token =
      base::UnguessableToken::DeserializeFromString(
          global_id_proto.frame_token());
  if (!frame_token) {
    std::move(continue_callback).Run(std::move(data));
    return;
  }
  autofill::FieldGlobalId global_id = {
      autofill::LocalFrameToken(*frame_token),
      autofill::FieldRendererId(global_id_proto.renderer_id())};

  // Look up the `global_id` in the main frame's manager. In the vast majority
  // of cases, this suffices because the AutofillDriverRouter routes the forms
  // to the main frame's manager. Since this is only used by internal
  // extensions, the edge case in which the main frame's form may not yet be
  // fully parsed is neglected.
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
  if (!autofill_driver) {
    std::move(continue_callback).Run(std::move(data));
    return;
  }
  autofill::FormStructure* form_structure =
      autofill_driver->GetAutofillManager().FindCachedFormById(global_id);
  if (!form_structure) {
    std::move(continue_callback).Run(std::move(data));
    return;
  }
  *data->mutable_form_data() = autofill::ToFormDataProto(
      form_structure->ToFormData(),
      autofill::FormDataProtoConversionReason::kExtensionAPI);
  std::move(continue_callback).Run(std::move(data));
}
#endif

std::string EncodePngOnBackgroundThread(const SkBitmap& bitmap) {
  TRACE_EVENT0("browser", "EncodePngOnBackgroundThread");
  std::optional<std::vector<uint8_t>> data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  if (!data) {
    return std::string();
  }
  return base::Base64Encode(data.value());
}

void OnEncodePng(AiDataKeyedService::AiDataCallback continue_callback,
                 std::string base64_result) {
  TRACE_EVENT0("browser", "OnEncodePng");
  auto ai_data = std::make_optional<AiDataKeyedService::BrowserData>();
  if (!base64_result.empty()) {
    ai_data->mutable_page_context()->set_tab_screenshot(base64_result);
  }
  std::move(continue_callback).Run(std::move(ai_data));
}

void OnGetTabScreenshotForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    const SkBitmap& bitmap) {
  TRACE_EVENT0("browser", "OnGetTabScreenshotForModelPrototyping");
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EncodePngOnBackgroundThread, base::OwnedRef(bitmap)),
      base::BindOnce(&OnEncodePng, std::move(continue_callback)));
}

void GetTabScreenshotForModelPrototyping(
    content::WebContents* web_contents,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT0("browser", "GetTabScreenshotForModelPrototyping");
  content::RenderWidgetHostView* const view =
      web_contents ? web_contents->GetRenderWidgetHostView() : nullptr;
  if (!view) {
    return std::move(continue_callback).Run(std::nullopt);
  }
  SkBitmap empty;
  view->CopyFromSurface(
      gfx::Rect(),  // Copy entire surface area.
      gfx::Size(),  // Result contains device-level detail.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnGetTabScreenshotForModelPrototyping,
                         std::move(continue_callback)),
          base::OwnedRef(empty)));
}

void GetSiteEngagementScoresForModelPrototyping(
    content::BrowserContext* browser_context,
    AiDataKeyedService::AiDataCallback continue_callback) {
  TRACE_EVENT0("browser", "GetSiteEngagementScoresForModelPrototyping");
  site_engagement::SiteEngagementService* service =
      site_engagement::SiteEngagementService::Get(browser_context);
  AiDataKeyedService::AiData data =
      std::make_optional<AiDataKeyedService::BrowserData>();
  if (!service) {
    return std::move(continue_callback).Run(std::move(data));
  }
  // Exclude webUI.
  std::vector<site_engagement::mojom::SiteEngagementDetails> scores =
      service->GetAllDetails(
          site_engagement::SiteEngagementService::URLSets::HTTP);

  auto* engagement_info = data->mutable_site_engagement();
  for (const auto& info : scores) {
    auto* entry = engagement_info->add_entries();
    entry->set_url(info.origin.spec());
    entry->set_score(info.total_score);
  }

  return std::move(continue_callback).Run(std::move(data));
}

std::unique_ptr<optimization_guide::proto::PageContextSpecifier>
CreateDefaultPageContextSpecifier(int dom_node_id) {
  auto page_context_specifier =
      std::make_unique<optimization_guide::proto::PageContextSpecifier>();
  page_context_specifier->set_inner_text(true);
  page_context_specifier->set_inner_text_dom_node_id(dom_node_id);
  page_context_specifier->set_tab_screenshot(true);
  page_context_specifier->set_ax_tree(true);
  page_context_specifier->set_pdf_data(true);
  return page_context_specifier;
}

// Fills synchronous information and kicks off concurrent tasks to fill an
// AiData.
void GetModelPrototypingAiData(AiDataKeyedService::AiDataSpecifier specifiers,
                               content::WebContents* web_contents,
                               AiDataKeyedService::AiDataCallback callback) {
  TRACE_EVENT0("browser", "GetModelPrototypingAiData");
  DCHECK(web_contents);

  // Fill data with synchronous information.
  AiDataKeyedService::BrowserData data;
  data.mutable_page_context()->set_url(
      web_contents->GetLastCommittedURL().spec());
  data.mutable_page_context()->set_title(
      base::UTF16ToUTF8(web_contents->GetTitle()));

  base::ConcurrentCallbacks<AiDataKeyedService::AiData> concurrent;
  auto page_context_specifier = specifiers.browser_data_collection_specifier()
                                    .foreground_tab_page_context_specifier();
  if (page_context_specifier.default_data()) {
    auto default_specifier = CreateDefaultPageContextSpecifier(
        page_context_specifier.inner_text_dom_node_id());
    page_context_specifier.CopyFrom(*default_specifier);
  }
  // TODO(https://crbug.com/385777825) check this specifier. For now always
  // collect it.
  GetAIPageContentForModelPrototyping(web_contents,
                                      concurrent.CreateCallback());
  GetAIPageContentWithActionableElementsForModelPrototyping(
      web_contents, concurrent.CreateCallback());
  if (page_context_specifier.ax_tree()) {
    RequestAxTreeSnapshotForModelPrototyping(web_contents,
                                             concurrent.CreateCallback());
  }
  if (page_context_specifier.inner_text()) {
    GetInnerTextForModelPrototyping(
        page_context_specifier.inner_text_dom_node_id(), web_contents,
        concurrent.CreateCallback());
  }
  if (page_context_specifier.tab_screenshot()) {
    GetTabScreenshotForModelPrototyping(web_contents,
                                        concurrent.CreateCallback());
  }

  if (specifiers.browser_data_collection_specifier()
          .has_history_query_specifiers()) {
    GetHistoryQueryResultForModelPrototyping(
        web_contents,
        specifiers.browser_data_collection_specifier()
            .history_query_specifiers(),
        concurrent.CreateCallback());
  }
#if !BUILDFLAG(IS_ANDROID)
  // TODO(https://crbug.com/385777825): generalize this logic and support other
  // page contexts for tabs.
  auto tab_specifier =
      specifiers.browser_data_collection_specifier().tabs_context_specifier();
  if (tab_specifier.has_general_tab_specifier()) {
    // All tabs metadata is collected, but the tab limit is only used to
    // determine if inner text should be collected.
    auto general_tab_specifier = tab_specifier.general_tab_specifier();
    int tabs_for_inner_text =
        general_tab_specifier.page_context_specifier().inner_text()
            ? general_tab_specifier.tab_limit()
            : 0;
    GetTabDataForModelPrototyping(tabs_for_inner_text, web_contents,
                                  concurrent);
  }
  if (page_context_specifier.has_field_global_id()) {
    GetFormDataByFieldGlobalIdForModelPrototyping(
        web_contents, page_context_specifier.field_global_id(),
        concurrent.CreateCallback());
  }
#endif
#if BUILDFLAG(ENABLE_PDF)
  if (page_context_specifier.pdf_data()) {
    RequestPdfBytesForModelPrototyping(web_contents,
                                       concurrent.CreateCallback());
  }
#endif  // BUILDFLAG(ENABLE_PDF)
  if (specifiers.browser_data_collection_specifier().site_engagement()) {
    GetSiteEngagementScoresForModelPrototyping(
        web_contents->GetBrowserContext(), concurrent.CreateCallback());
  }
  std::move(concurrent)
      .Done(base::BindOnce(&OnDataCollectionsComplete, std::move(callback),
                           std::move(data)));
}
#if BUILDFLAG(ENABLE_GLIC)
glic::mojom::GetTabContextOptions DefaultOptions() {
  glic::mojom::GetTabContextOptions options;
  options.include_annotated_page_content = true;
  options.include_viewport_screenshot = true;
  return options;
}

#endif  // BUILDFLAG(ENABLE_GLIC)

void RunLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

// Feature to add allow listed extensions remotely for data collection.
BASE_FEATURE(kAllowlistedAiDataExtensions,
             "AllowlistedAiDataExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAllowlistedExtensionsForData{
    &kAllowlistedAiDataExtensions, "allowlisted_extension_ids",
    /*default_value=*/""};

const base::FeatureParam<std::string> kBlocklistedExtensionsForData{
    &kAllowlistedAiDataExtensions, "blocked_extension_ids",
    /*default_value=*/""};

// Feature to add allow listed extensions remotely for actions.
BASE_FEATURE(kAllowlistedActionsExtensions,
             "AllowlistedActionsExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAllowlistedExtensionsForActions{
    &kAllowlistedActionsExtensions, "allowlisted_extension_ids",
    /*default_value=*/""};

const base::FeatureParam<std::string> kBlocklistedExtensionsForActions{
    &kAllowlistedActionsExtensions, "blocked_extension_ids",
    /*default_value=*/""};

}  // namespace

AiDataKeyedService::AiDataKeyedService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AiDataKeyedService::~AiDataKeyedService() = default;

const base::Feature&
AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting() {
  return kAllowlistedAiDataExtensions;
}

const base::Feature&
AiDataKeyedService::GetAllowlistedActionsExtensionsFeatureForTesting() {
  return kAllowlistedActionsExtensions;
}

void AiDataKeyedService::GetAiData(int dom_node_id,
                                   content::WebContents* web_contents,
                                   std::string user_input,
                                   AiDataCallback callback,
                                   int tabs_for_inner_text) {
  TRACE_EVENT0("browser", "AiDataKeyedService::GetAiData");
  // Configure a default set of specifier.
  AiDataSpecifier specifier;
  auto* browser_data_collection_specifier =
      specifier.mutable_browser_data_collection_specifier();
  browser_data_collection_specifier
      ->set_allocated_foreground_tab_page_context_specifier(
          CreateDefaultPageContextSpecifier(dom_node_id).release());

  auto* general_tabs_context_specifier =
      browser_data_collection_specifier->mutable_tabs_context_specifier()
          ->mutable_general_tab_specifier();
  general_tabs_context_specifier->mutable_page_context_specifier()
      ->set_inner_text(true);
  general_tabs_context_specifier->set_tab_limit(tabs_for_inner_text);

  browser_data_collection_specifier->set_site_engagement(true);
  browser_data_collection_specifier->set_tab_groups(true);

  GetAiDataWithSpecifier(web_contents, std::move(specifier),
                         std::move(callback));
}

void AiDataKeyedService::GetAiDataWithSpecifier(
    content::WebContents* web_contents,
    AiDataSpecifier specifier,
    AiDataCallback callback) {
  TRACE_EVENT0("browser", "AiDataKeyedService::GetAiDataWithSpecifier");
  GetModelPrototypingAiData(std::move(specifier), web_contents,
                            std::move(callback));
}

bool AiDataKeyedService::IsExtensionAllowlistedForData(
    const std::string& extension_id) {
  std::vector<std::string> blocklisted_extensions =
      base::SplitString(kBlocklistedExtensionsForData.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(blocklisted_extensions, extension_id)) {
    return false;
  }

  static const base::NoDestructor<std::vector<std::string>>
      kHardcodedAllowlistedExtensions({// https://issues.chromium.org/373645534
                                       "hpkopmikdojpadgmioifjjodbmnjjjca",
                                       // https://issues.chromium.org/377129777
                                       "bgbpcgpcobgjpnpiginpidndjpggappi",
                                       // https://issues.chromium.org/376699519
                                       "eefninhhiifgcimjkmkongegpoaikmhm",
                                       // https://issues.chromium.org/393435942
                                       "fjhpgileahdpnmfmaggobehbipojhlce",
                                       // https://issues.chromium.org/403366603
                                       "abdciamfdmknaeggbnmafmbdfdmhfgfa",
                                       // https://issues.chromium.org/414437025
                                       "fiamdfnbelfkjlacoaeiclobkdmckaoa"});
  if (base::Contains(*kHardcodedAllowlistedExtensions, extension_id)) {
    return true;
  }

  std::vector<std::string> allowlisted_extensions =
      base::SplitString(kAllowlistedExtensionsForData.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(allowlisted_extensions, extension_id)) {
    return true;
  }

  return false;
}

bool AiDataKeyedService::IsExtensionAllowlistedForActions(
    const std::string& extension_id) {
  std::vector<std::string> blocklisted_extensions =
      base::SplitString(kBlocklistedExtensionsForActions.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(blocklisted_extensions, extension_id)) {
    return false;
  }

  static const base::NoDestructor<std::vector<std::string>>
      kHardcodedAllowlistedExtensions({});
  if (base::Contains(*kHardcodedAllowlistedExtensions, extension_id)) {
    return true;
  }

  std::vector<std::string> allowlisted_extensions =
      base::SplitString(kAllowlistedExtensionsForActions.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (base::Contains(allowlisted_extensions, extension_id)) {
    return true;
  }

  return false;
}

bool AiDataKeyedService::IsExtensionAllowlistedForStable(
    const std::string& extension_id) {
  // Stable channel always requires --experimental-ai-stable-channel flag.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kExperimentalAiStableChannel)) {
    return false;
  }

  // And the extension must be on this list.
  static const base::NoDestructor<std::vector<std::string>>
      kStableChannelAllowlistedIds({});
  return base::Contains(*kStableChannelAllowlistedIds, extension_id);
}

void AiDataKeyedService::StartTask(
    optimization_guide::proto::BrowserStartTask task,
    base::OnceCallback<void(optimization_guide::proto::BrowserStartTaskResult)>
        callback) {
#if !BUILDFLAG(ENABLE_GLIC)
  RunLater(base::BindOnce(std::move(callback),
                          optimization_guide::proto::BrowserStartTaskResult()));
  return;
#else
  if (actor_coordinator_) {
    VLOG(1) << "Start Task failed: Actor coordinator already exists and only "
               "one allowed at a time.";
    optimization_guide::proto::BrowserStartTaskResult result;
    result.set_status(
        optimization_guide::proto::BrowserStartTaskResult::OVER_TASK_LIMIT);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  actor_coordinator_ = std::make_unique<actor::ActorCoordinator>(profile);
  // The GlicKeyedService is normally what sets up the profile for Glic, but
  // GlicKeyedService is not compabible with system profiles, so we need to
  // manually register the profile here to make sure that OptimizationGuide is
  // properly set up.
  // Note that this function is idempotent, so it is fine to call it multiple
  // times.
  actor::ActorCoordinator::RegisterWithProfile(profile);
  task_needs_navigate_ = true;
  // TODO(https://crbug.com/407860715): Implement a separate host API to start
  // a task, and remove action handling here.
  optimization_guide::proto::BrowserAction dummy_navigate_action;
  dummy_navigate_action.add_action_information()->mutable_navigate();
  actor_coordinator_->StartTask(
      dummy_navigate_action,
      base::BindOnce(&AiDataKeyedService::OnTaskCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback), task_id_,
                     tab_id_),
      task.has_tab_id() ? std::make_optional(tabs::TabHandle(task.tab_id()))
                        : std::nullopt);
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void AiDataKeyedService::StopTask(int64_t task_id,
                                  base::OnceCallback<void(bool)> callback) {
#if !BUILDFLAG(ENABLE_GLIC)
  RunLater(base::BindOnce(std::move(callback), false));
  return;
#else
  if (task_id != task_id_ || !actor_coordinator_ || !tab_) {
    VLOG(1)
        << "Stop Task failed: Task id or tab id does not match current task.";
    RunLater(base::BindOnce(std::move(callback), false));
    return;
  }

  actor_coordinator_.reset();
  tab_.reset();
  task_id_ += 1;
  tab_id_ += 1;
  RunLater(base::BindOnce(std::move(callback), true));
#endif  // BUILDFLAG(ENABLE_GLIC)
}

void AiDataKeyedService::ExecuteAction(
    optimization_guide::proto::BrowserAction action,
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback) {
#if !BUILDFLAG(ENABLE_GLIC)
  RunLater(base::BindOnce(std::move(callback),
                          optimization_guide::proto::BrowserActionResult()));
  return;
#else
  if (task_id_ != action.task_id() || tab_id_ != action.tab_id()) {
    VLOG(1) << "Execute Action failed: Task id or tab id does not match "
               "current task.";
    optimization_guide::proto::BrowserActionResult result;
    result.set_action_result(0);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  if (task_needs_navigate_) {
    task_needs_navigate_ = false;
    if (action.action_information_size() != 1 ||
        !action.action_information(0).has_navigate()) {
      VLOG(1) << "Execute Action failed: Action is not a navigate action and "
                 "is the first action in the task.";
      optimization_guide::proto::BrowserActionResult result;
      result.set_action_result(0);
      RunLater(base::BindOnce(std::move(callback), std::move(result)));
      return;
    }
  }
  actor_coordinator_->Act(
      std::move(action),
      base::BindOnce(&AiDataKeyedService::OnActionFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback), task_id_,
                     tab_id_));
#endif  // BUILDFLAG(ENABLE_GLIC)
}

bool AiDataKeyedService::IsActorCoordinatorActingOnTab(
    const content::WebContents* tab) const {
#if BUILDFLAG(ENABLE_GLIC)
  return actor_coordinator_ && actor_coordinator_->HasTaskForTab(tab);
#else
  return false;
#endif
}

#if BUILDFLAG(ENABLE_GLIC)
void AiDataKeyedService::OnTaskCreated(
    base::OnceCallback<void(optimization_guide::proto::BrowserStartTaskResult)>
        callback,
    int task_id,
    int tab_id,
    base::WeakPtr<tabs::TabInterface> tab) {
  optimization_guide::proto::BrowserStartTaskResult result;
  if (!tab || tab_id_ != tab_id || task_id_ != task_id) {
    VLOG(1)
        << "Start Task failed: Tab id or task id does not match current task.";
    result.set_status(
        optimization_guide::proto::BrowserStartTaskResult::OVER_TASK_LIMIT);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  tab_ = tab;
  result.set_task_id(task_id);
  result.set_tab_id(tab_id);
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  RunLater(base::BindOnce(std::move(callback), std::move(result)));
}

void AiDataKeyedService::OnActionFinished(
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback,
    int task_id,
    int tab_id,
    actor::mojom::ActionResultPtr action_result) {
  if (!tab_ || tab_id_ != tab_id || task_id_ != task_id) {
    VLOG(1) << "Execute Action failed: Tab id or task id does not match "
               "current task.";
    optimization_guide::proto::BrowserActionResult result;
    result.set_action_result(0);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  // TODO(https://crbug.com/398271171): Remove when the actor coordinator
  // handles getting a new observation.

  glic::FocusedTabData focused_tab_data{tab_->GetContents()->GetWeakPtr()};
  glic::FetchPageContext(
      focused_tab_data, DefaultOptions(),
      base::BindOnce(&AiDataKeyedService::ConvertToBrowserActionResult,
                     weak_factory_.GetWeakPtr(), std::move(callback), task_id_,
                     tab_id_, std::move(action_result)));
}

void AiDataKeyedService::ConvertToBrowserActionResult(
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback,
    int task_id,
    int tab_id,
    actor::mojom::ActionResultPtr action_result,
    glic::mojom::GetContextResultPtr context_result) {
  optimization_guide::proto::BrowserActionResult browser_action_result;
  if (task_id != task_id_ || tab_id != tab_id_ ||
      context_result->is_error_reason()) {
    VLOG(1) << "Execute Action failed: Tab id or task id does not match "
               "current task.";
    browser_action_result.set_action_result(0);
    RunLater(
        base::BindOnce(std::move(callback), std::move(browser_action_result)));
    return;
  }
  if (context_result->get_tab_context() &&
      context_result->get_tab_context()->annotated_page_data &&
      context_result->get_tab_context()
          ->annotated_page_data->annotated_page_content) {
    auto apc = context_result->get_tab_context()
                   ->annotated_page_data->annotated_page_content.value()
                   .As<optimization_guide::proto::AnnotatedPageContent>();
    if (apc.has_value()) {
      auto apc_value = *std::move(apc);
      browser_action_result.mutable_annotated_page_content()->Swap(&apc_value);
    }
  }
  if (context_result->get_tab_context()->viewport_screenshot &&
      context_result->get_tab_context()->viewport_screenshot->data.size() !=
          0) {
    auto& data = context_result->get_tab_context()->viewport_screenshot->data;
    browser_action_result.set_screenshot(data.data(), data.size());
    browser_action_result.set_screenshot_mime_type(
        context_result->get_tab_context()->viewport_screenshot->mime_type);
  }
  browser_action_result.set_task_id(task_id);
  browser_action_result.set_tab_id(tab_id);
  browser_action_result.set_action_result(actor::IsOk(*action_result) ? 1 : 0);
  RunLater(
      base::BindOnce(std::move(callback), std::move(browser_action_result)));
}
#endif  // BUILDFLAG(ENABLE_GLIC)
