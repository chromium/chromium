// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/base_tracing.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/profiles/profile.h"
#include "components/compose/buildflags.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

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
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
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

// Fills synchronous information and kicks off concurrent tasks to fill an
// AiData.
void GetModelPrototypingAiData(int tabs_for_inner_text,
                               int dom_node_id,
                               content::WebContents* web_contents,
                               std::string user_input,
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
  RequestAxTreeSnapshotForModelPrototyping(web_contents,
                                           concurrent.CreateCallback());
  GetInnerTextForModelPrototyping(dom_node_id, web_contents,
                                  concurrent.CreateCallback());
  GetTabScreenshotForModelPrototyping(web_contents,
                                      concurrent.CreateCallback());
#if !BUILDFLAG(IS_ANDROID)
  GetTabDataForModelPrototyping(tabs_for_inner_text, web_contents, concurrent);
#endif
  GetSiteEngagementScoresForModelPrototyping(web_contents->GetBrowserContext(),
                                             concurrent.CreateCallback());
  std::move(concurrent)
      .Done(base::BindOnce(&OnDataCollectionsComplete, std::move(callback),
                           std::move(data)));
}

// Feature to add allow listed extensions remotely.
BASE_FEATURE(kAllowlistedAiDataExtensions,
             "AllowlistedAiDataExtensions",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kAllowlistedExtensions{
    &kAllowlistedAiDataExtensions, "allowlisted_extension_ids",
    /*default_value=*/""};

const base::FeatureParam<std::string> kBlocklistedExtensions{
    &kAllowlistedAiDataExtensions, "blocked_extension_ids",
    /*default_value=*/""};

}  // namespace

AiDataKeyedService::AiDataKeyedService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AiDataKeyedService::~AiDataKeyedService() = default;

const base::Feature&
AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting() {
  return kAllowlistedAiDataExtensions;
}

void AiDataKeyedService::GetAiData(int dom_node_id,
                                   content::WebContents* web_contents,
                                   std::string user_input,
                                   AiDataCallback callback) {
  TRACE_EVENT0("browser", "AiDataKeyedService::GetAiData");
  GetAiDataWithSpecifiers(10, dom_node_id, web_contents, user_input,
                            std::move(callback));
}

void AiDataKeyedService::GetAiDataWithSpecifiers(
    int tabs_for_inner_text,
    int dom_node_id,
    content::WebContents* web_contents,
    std::string user_input,
    AiDataCallback callback) {
  TRACE_EVENT0("browser", "AiDataKeyedService::GetAiDataWithSpecifiers");
  GetModelPrototypingAiData(tabs_for_inner_text, dom_node_id, web_contents,
                            user_input, std::move(callback));
}

std::vector<std::string> AiDataKeyedService::GetAllowlistedExtensions() {
  std::vector<std::string> allowlisted_extensions =
      base::SplitString(kAllowlistedExtensions.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  static const std::vector<std::string> kHardcodedAllowlistedExtensions = {
      // https://issues.chromium.org/373645534
      "hpkopmikdojpadgmioifjjodbmnjjjca",
      // https://issues.chromium.org/373462321
      "nfdaijodggdcjengofmbibbkcnopmikg"};
  allowlisted_extensions.insert(allowlisted_extensions.end(),
                                kHardcodedAllowlistedExtensions.begin(),
                                kHardcodedAllowlistedExtensions.end());
  std::vector<std::string> blocklisted_extensions =
      base::SplitString(kBlocklistedExtensions.Get(), ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  auto it = std::remove_if(
      allowlisted_extensions.begin(), allowlisted_extensions.end(),
      [&](const std::string& extension_id) {
        return base::Contains(blocklisted_extensions, extension_id);
      });
  allowlisted_extensions.erase(it, allowlisted_extensions.end());

  return allowlisted_extensions;
}
