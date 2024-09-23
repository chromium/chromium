// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_preview_ui_wrapper.h"

#include "base/containers/flat_map.h"
#include "base/containers/id_map.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/print_job_constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromeos {

namespace {

// IDMap instance for this particular instance.
base::LazyInstance<base::IDMap<PrintPreviewUiWrapper*>>::DestructorAtExit
    g_print_preview_ui_id_map = LAZY_INSTANCE_INITIALIZER;

// Mapping from PrintPreviewUI ID to print preview request ID.
// This mapping is used in static accessors to track which print preview
// instance is available and which requests are associated with it.
// It's possible that a print preview instance is destroyed by the time
// preview generation is completed, so this also serves as a way to keep track
// of the available instances.
using PrintPreviewRequestIdMap = base::flat_map<int, int>;

PrintPreviewRequestIdMap& GetPrintPreviewRequestIdMap() {
  static base::NoDestructor<PrintPreviewRequestIdMap> map;
  return *map;
}

}  // namespace

PrintPreviewUiWrapper::PrintPreviewUiWrapper() {
  SetPreviewUIId();
}

// Clean up of member variables should be done via Reset() prior to destruction.
PrintPreviewUiWrapper::~PrintPreviewUiWrapper() = default;

void PrintPreviewUiWrapper::Reset() {
  render_frame_host_ = nullptr;
  receiver_.reset();
  print_render_frame_.reset();

  if (!id_) {
    // `id_` can be empty if no preview generation was requested.
    return;
  }

  g_print_preview_ui_id_map.Get().Remove(*id_);
  GetPrintPreviewRequestIdMap().erase(*id_);
}

void PrintPreviewUiWrapper::BindPrintPreviewUI(content::RenderFrameHost* rfh) {
  CHECK(!render_frame_host_);
  render_frame_host_ = rfh;
  if (!print_render_frame_.is_bound()) {
    render_frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(
        &print_render_frame_);
  }

  if (!receiver_.is_bound()) {
    print_render_frame_->SetPrintPreviewUI(
        receiver_.BindNewEndpointAndPassRemote());
  }
}

// static
bool PrintPreviewUiWrapper::ShouldCancelRequest(
    const std::optional<int32_t>& print_preview_ui_id,
    int request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // A null `print_preview_ui_id` indicates that the preview request for the
  // associated ID is not in progress and can be cancelled.
  if (!print_preview_ui_id) {
    return true;
  }

  auto& map = GetPrintPreviewRequestIdMap();
  auto it = map.find(*print_preview_ui_id);
  return it == map.end() || request_id != it->second;
}

void PrintPreviewUiWrapper::GeneratePrintPreview(base::Value::Dict settings) {
  CHECK(print_render_frame_.is_bound());
  // TODO(jimmyxgong): Store settings for sticky settings support.
  // Store the request ID.
  const int request_id = settings.FindInt(printing::kPreviewRequestID).value();
  GetPrintPreviewRequestIdMap()[*id_] = request_id;
  print_render_frame_->PrintPreview(settings.Clone());
}

// TODO(jimmyxgong): Implement stub.
void PrintPreviewUiWrapper::SetOptionsFromDocument(
    ::printing::mojom::OptionsFromDocumentParamsPtr params,
    int32_t request_id) {}

void PrintPreviewUiWrapper::DidPrepareDocumentForPreview(
    int32_t document_cookie,
    int32_t request_id) {}

void PrintPreviewUiWrapper::DidPreviewPage(
    ::printing::mojom::DidPreviewPageParamsPtr params,
    int32_t request_id) {}

void PrintPreviewUiWrapper::MetafileReadyForPrinting(
    ::printing::mojom::DidPreviewDocumentParamsPtr params,
    int32_t request_id) {}

void PrintPreviewUiWrapper::PrintPreviewFailed(int32_t document_cookie,
                                               int32_t request_id) {}

void PrintPreviewUiWrapper::PrintPreviewCancelled(int32_t document_cookie,
                                                  int32_t request_id) {}

void PrintPreviewUiWrapper::PrinterSettingsInvalid(int32_t document_cookie,
                                                   int32_t request_id) {}

void PrintPreviewUiWrapper::DidGetDefaultPageLayout(
    ::printing::mojom::PageSizeMarginsPtr page_layout_in_points,
    const gfx::RectF& printable_area_in_points,
    bool all_pages_have_custom_size,
    bool all_pages_have_custom_orientation,
    int32_t request_id) {}

void PrintPreviewUiWrapper::DidStartPreview(
    ::printing::mojom::DidStartPreviewParamsPtr params,
    int32_t request_id) {}

void PrintPreviewUiWrapper::SetPreviewUIId() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!id_);

  id_ = g_print_preview_ui_id_map.Get().Add(this);
  GetPrintPreviewRequestIdMap()[*id_] = -1;
}

}  // namespace chromeos
