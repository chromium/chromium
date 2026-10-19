// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drag_and_drop_util.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_provider_metrics.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/filename_util.h"

namespace glic {
namespace {

struct DragSourceInfo {
  base::UnguessableToken drag_id;
  bool is_eligible = false;
  base::WeakPtr<content::BrowserContext> source_browser_context;
  // The frame the drag started in. This is held for the whole drag and
  // resolved at drop time to build the enterprise policy check's source
  // endpoint.
  //
  // It is a `GlobalRenderFrameHostToken` rather than a
  // `GlobalRenderFrameHostId` because the id's routing component is recycled
  // once a RenderFrameHost is destroyed, so a stale id could resolve to an
  // unrelated, live frame. Tokens are never reused for the lifetime of the
  // browser process. See content/public/browser/render_frame_host.h.
  //
  // A token identifies a RenderFrameHost, not a document, so it does not pin
  // the drop to the document the drag started in:
  // - A same-site navigation can reuse the RenderFrameHost for the new
  //   document. The token then resolves to the same frame showing a different
  //   page.
  // - A cross-document navigation can put the old RenderFrameHost into the
  //   back/forward cache. The token then still resolves to the original
  //   document, but that document is no longer active.
  // Neither case affects page context eligibility, which is captured in
  // `is_eligible` at drag start. The policy check, however, reads
  // `source_rfh->GetMainFrame()->GetLastCommittedURL()` at drop time: in the
  // first case it sees the new page's URL, while in the second case it still
  // sees the original page's URL (and the invoke still succeeds while the
  // frame remains in the back/forward cache).
  content::GlobalRenderFrameHostToken source_rfh_token;
  tabs::TabHandle source_tab_handle;
};

// Tracks the drag-start page context eligibility and source routing metadata
// of the most recent web drag, keyed by `DropData::drag_id`. Modeled after
// `GlicClipboardEligibilityMonitor` in
// `chrome/browser/glic/host/guest_util.cc`, which tracks the most recent
// copy's eligibility keyed by clipboard sequence number.
class GlicDragEligibilityMonitor {
 public:
  static GlicDragEligibilityMonitor* GetInstance() {
    static base::NoDestructor<GlicDragEligibilityMonitor> instance;
    return instance.get();
  }

  GlicDragEligibilityMonitor() = default;
  ~GlicDragEligibilityMonitor() = default;

  void RecordDragStart(DragSourceInfo info) { last_drag_ = std::move(info); }

  std::optional<DragSourceInfo> TakeDragSourceInfo(
      const base::UnguessableToken& drag_id) {
    if (!last_drag_ || last_drag_->drag_id != drag_id) {
      return std::nullopt;
    }
    return std::exchange(last_drag_, std::nullopt);
  }

 private:
  std::optional<DragSourceInfo> last_drag_;
};

// Constructs the `glic::mojom::AdditionalContext` containing the dropped data.
// Needed to bundle all relevant multimodal web content (raw file bytes,
// source tab IDs, filenames, and metadata) into a single payload conforming to
// Glic's Mojo protocol before triggering the Glic Invoke flow. `TabContext` is
// deliberately omitted: it was previously attached only so the web client could
// validate page context eligibility on the dropped content, which is no longer
// needed now that Chrome enforces eligibility before invoking.
glic::mojom::AdditionalContextPtr BuildDragDropAdditionalContext(
    const content::DropData& drop_data,
    tabs::TabHandle source_tab_handle) {
  auto context = glic::mojom::AdditionalContext::New();
  context->source = glic::mojom::AdditionalContextSource::kWebDragDrop;

  std::vector<glic::mojom::AdditionalContextPartPtr> parts;

  auto context_data = mojom::ContextData::New();
  // TODO(b/448726704): update to use an Image part.
  context_data->mime_type = "image/png";
  context_data->data =
      mojo_base::BigBuffer(base::as_byte_span(drop_data.file_contents));

  std::string suggested_filename;
  if (std::optional<base::FilePath> filename =
          drop_data.GetSafeFilenameForImageFileContents()) {
    suggested_filename = filename->AsUTF8Unsafe();
  } else {
    suggested_filename = "image.png";
  }
  context_data->filename = std::move(suggested_filename);

  parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));

  if (!drop_data.url_infos.empty()) {
    context->name = drop_data.url_infos.front().url.spec();
  }

  context->tab_id = source_tab_handle.raw_value();
  context->parts = std::move(parts);
  return context;
}

// Resolves Glic instance, configures invocation options,
// and initiates the asynchronous Glic Invoke call.
// Needed to find the specific destination `GlicInstance` hosting the Glic panel
// where the user dropped the content, target that conversation specifically,
// and activate the Glic panels using the assembled drop payload.
void TriggerDragDropInvoke(content::WebContents* target_web_contents,
                           content::GlobalRenderFrameHostId source_rfh_id,
                           tabs::TabHandle source_tab_handle,
                           glic::mojom::AdditionalContextPtr context,
                           GlicMetricReporter reporter) {
  Profile* profile =
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext());
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  if (!service) {
    return;
  }

  GlicInvokeOptions invoke_options(mojom::InvocationSource::kWebDragDrop);
  invoke_options.additional_context = AdditionalTabContext(
      std::move(context), source_rfh_id, PolicyCheck::kClipboard);

  // We specify the panel's Glic WebContents as the target surface.
  GlicInstance* target_instance =
      service->instance_coordinator().GetInstanceWithGlicWebContents(
          target_web_contents);

  if (target_instance) {
    invoke_options.target = target_instance->GetInvokeTarget(
        glic::Target::Surface(source_tab_handle));
  } else {
    invoke_options.target.surface = source_tab_handle;
  }

  reporter.RecordEnumeration("DragAndDrop.ValidationResult2",
                             GlicDragAndDropValidationResult::kSuccess);
  service->Invoke(std::move(invoke_options));
}

// Restores drag-and-drop metadata (original source URL and file extensions)
// that operating systems frequently strip during drag-and-drop actions. Needed
// because OS pasteboards / clipboards are lossy channels (often stripping
// source URLs for privacy or converting image formats to generic types like
// TIFF). When `url_infos` is present on `drop_data`, this recovers the source
// URL and infers the missing file extension from it.
content::DropData RestoreDragMetadata(const content::DropData& drop_data) {
  content::DropData augmented_drop_data = drop_data;
  if (!drop_data.url_infos.empty()) {
    augmented_drop_data.file_contents_source_url =
        drop_data.url_infos.front().url;
    if (augmented_drop_data.file_contents_filename_extension.empty()) {
      base::FilePath path =
          net::GenerateFileName(augmented_drop_data.file_contents_source_url,
                                /*content_disposition=*/std::string(),
                                /*referrer_charset=*/std::string(),
                                /*suggested_name=*/std::string(),
                                /*mime_type=*/std::string(),
                                /*default_name=*/std::string());
      base::FilePath::StringType ext = path.Extension();
      if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
      }
      augmented_drop_data.file_contents_filename_extension = ext;
    }
  }
  return augmented_drop_data;
}

// Returns the content type of the drag. Note that web drags of images or files
// usually include fallback formats (like plain text or HTML markup of the tag).
// We check in priority order (Image > File > URL > HTML > Text) to gauge the
// primary/richest format the user intended to drop, instead of classifying
// all drops as kMultipleTypes.
GlicDragAndDropContentType GetDragAndDropContentType(
    const content::DropData& drop_data) {
  // `file_contents` is populated by Blink for in-memory image data dragged from
  // web pages.
  if (!drop_data.file_contents.empty()) {
    return GlicDragAndDropContentType::kImage;
  }
  if (!drop_data.filenames.empty()) {
    return GlicDragAndDropContentType::kFile;
  }
  if (!drop_data.url_infos.empty()) {
    return GlicDragAndDropContentType::kUrl;
  }
  if (drop_data.html && !drop_data.html->empty()) {
    return GlicDragAndDropContentType::kHtml;
  }
  if (drop_data.text && !drop_data.text->empty()) {
    return GlicDragAndDropContentType::kText;
  }
  return GlicDragAndDropContentType::kUnknown;
}

}  // namespace

bool IsChromeWebDrag(const content::DropData& drop_data) {
  return drop_data.drag_id.has_value();
}

GlicWebDragObserver::GlicWebDragObserver(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab) {}

GlicWebDragObserver::~GlicWebDragObserver() = default;

void GlicWebDragObserver::DidStartDrag(content::RenderFrameHost* source_rfh,
                                       const content::DropData& drop_data) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // `DidStartDrag` runs on every web drag in a tab, not only drags into Glic,
  // so avoid heavier work such as metrics recording here. If a drag is later
  // dropped onto Glic, `StartDragAndDropInvoke` validates it and records
  // `Glic.DragAndDrop.ValidationResult2`.
  if (!base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    return;
  }

  if (!source_rfh || !drop_data.drag_id.has_value() ||
      !source_rfh->IsActive()) {
    return;
  }

  content::BrowserContext* browser_context = source_rfh->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  bool is_eligible = false;
  if (GlicEnabling::IsProfileEligible(profile)) {
    auto* helper = tabs::PageContextEligibilityHelper::From(&tab());
    if (!helper) {
      return;
    }
    optimization_guide::PageContextEligibilityStatus status =
        helper->IsPageContextEligible();
    is_eligible =
        (status == optimization_guide::PageContextEligibilityStatus::kEligible);
  }

  GlicDragEligibilityMonitor::GetInstance()->RecordDragStart(DragSourceInfo{
      .drag_id = *drop_data.drag_id,
      .is_eligible = is_eligible,
      .source_browser_context = browser_context->GetWeakPtr(),
      .source_rfh_token = source_rfh->GetGlobalFrameToken(),
      .source_tab_handle = tab().GetHandle(),
  });
}

// Initiates the Glic drag-and-drop invocation workflow when a web drag is
// dropped onto the Glic panel.
void StartDragAndDropInvoke(content::WebContents* target_web_contents,
                            const content::DropData& drop_data) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!target_web_contents || !IsChromeWebDrag(drop_data)) {
    return;
  }

  GlicMetricReporter reporter(
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext()));

  // Record Content Type (intent/demand) immediately when the drop is
  // recognized.
  reporter.RecordEnumeration("DragAndDrop.ContentType",
                             GetDragAndDropContentType(drop_data));

  if (!base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kBlockedByFeatureFlag);
    return;
  }

  // TODO(crbug.com/567183211): Temporarily reject non-file web drags (such as
  // text and links) until extended drag-and-drop payload types are supported.
  if (drop_data.file_contents.empty()) {
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kUnsupportedContentType);
    return;
  }

  std::optional<DragSourceInfo> info =
      GlicDragEligibilityMonitor::GetInstance()->TakeDragSourceInfo(
          *drop_data.drag_id);
  if (!info) {
    // No record of this drag in `GlicDragEligibilityMonitor`, e.g. because the
    // drag source was not a tab or had no eligibility helper.
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kUnknownDragSource);
    return;
  }

  if (info->source_browser_context.get() !=
      target_web_contents->GetBrowserContext()) {
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kCrossProfileDragBlocked);
    return;
  }

  if (!info->is_eligible) {
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kPageContextIneligible);
    // TODO(b/552675444): Implement error signaling through Invoke for the
    // purpose of showing these toasts in the web client.
    return;
  }

  // `FromFrameToken` returns nullptr once the source frame has been destroyed,
  // e.g. because its tab closed or the frame was removed from the page. This
  // is also the liveness check relied upon by the enterprise policy check
  // downstream. See `DragSourceInfo::source_rfh_token` for what a live token
  // does and does not guarantee.
  content::RenderFrameHost* source_rfh =
      content::RenderFrameHost::FromFrameToken(info->source_rfh_token);
  if (!source_rfh) {
    reporter.RecordEnumeration(
        "DragAndDrop.ValidationResult2",
        GlicDragAndDropValidationResult::kSourceFrameDestroyed);
    return;
  }

  content::DropData augmented_drop_data = RestoreDragMetadata(drop_data);
  auto context = BuildDragDropAdditionalContext(augmented_drop_data,
                                                info->source_tab_handle);
  TriggerDragDropInvoke(target_web_contents, source_rfh->GetGlobalId(),
                        info->source_tab_handle, std::move(context), reporter);
}

}  // namespace glic
