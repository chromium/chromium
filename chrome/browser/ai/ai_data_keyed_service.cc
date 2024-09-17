// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_data_keyed_service.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/concurrent_callbacks.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/content_extraction/inner_text.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

// Fills an AiData proto with information from GetInnerText. If no result,
// returns an empty AiDAta.
void OnGetInnerTextForModelPrototyping(
    AiDataKeyedService::AiDataCallback continue_callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  AiDataKeyedService::AiData data;
  if (result) {
    data = std::make_optional<
        optimization_guide::proto::
            ModelPrototypingRequest_BrowserCollectedInformation>();
    data->set_inner_text(result->inner_text);
    if (result->node_offset) {
      data->set_inner_text_offset(result->node_offset.value());
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
  DCHECK(web_contents);
  DCHECK(web_contents->GetPrimaryMainFrame());

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
  AiDataKeyedService::AiData data;
  if (ax_tree_update.has_tree_data) {
    data = std::make_optional<
        optimization_guide::proto::
            ModelPrototypingRequest_BrowserCollectedInformation>();
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

// Fills synchronous information and kicks off concurrent tasks to fill an
// AiData.
void GetModelPrototypingAiData(int dom_node_id,
                               content::WebContents* web_contents,
                               std::string user_input,
                               AiDataKeyedService::AiDataCallback callback) {
  DCHECK(web_contents);

  // Fill data with synchronous information.
  optimization_guide::proto::ModelPrototypingRequest_BrowserCollectedInformation
      data;
  data.mutable_page_context()->set_url(
      web_contents->GetLastCommittedURL().spec());
  data.mutable_page_context()->set_title(
      base::UTF16ToUTF8(web_contents->GetTitle()));

  base::ConcurrentCallbacks<AiDataKeyedService::AiData> concurrent;
  RequestAxTreeSnapshotForModelPrototyping(web_contents,
                                           concurrent.CreateCallback());
  GetInnerTextForModelPrototyping(dom_node_id, web_contents,
                                  concurrent.CreateCallback());

  std::move(concurrent)
      .Done(base::BindOnce(&OnDataCollectionsComplete, std::move(callback),
                           std::move(data)));
}

}  // namespace

AiDataKeyedService::AiDataKeyedService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

AiDataKeyedService::~AiDataKeyedService() = default;

void AiDataKeyedService::GetAiData(int dom_node_id,
                                   content::WebContents* web_contents,
                                   std::string user_input,
                                   AiDataCallback callback) {
  GetModelPrototypingAiData(dom_node_id, web_contents, user_input,
                            std::move(callback));
}
