// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_drag_and_drop_util.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/filename_util.h"

namespace glic {

namespace {

const char16_t kGlicDragIdKey[] = u"chromium/x-drag-id";

// Constructs the `glic::mojom::AdditionalContext` containing the dropped data
// and the retrieved page context of the originating source tab.
// Needed to bundle all relevant multimodal web content (raw file bytes,
// source tab IDs, filenames, page structures, and metadata) into a single
// payload conforming to Glic's Mojo protocol before triggering the Glic Invoke
// flow.
glic::mojom::AdditionalContextPtr BuildDragDropAdditionalContext(
    const content::DropData& drop_data,
    tabs::TabInterface* source_tab,
    glic::mojom::TabContextPtr tab_context) {
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

  parts.push_back(
      mojom::AdditionalContextPart::NewTabContext(std::move(tab_context)));

  if (!drop_data.url_infos.empty()) {
    context->name = drop_data.url_infos.front().url.spec();
  }

  if (source_tab) {
    context->tab_id = source_tab->GetHandle().raw_value();
  }

  context->parts = std::move(parts);
  return context;
}

// Resolves Glic instance, configures invocation options,
// and initiates the asynchronous Glic Invoke call.
// Needed to find the specific destination `GlicInstance` hosting the Glic panel
// where the user dropped the content, target that conversation specifically,
// and activate the Glic panels using the assembled drop payload.
void TriggerDragDropInvoke(content::WebContents* target_web_contents,
                           content::RenderFrameHost* source_rfh,
                           tabs::TabInterface* source_tab,
                           glic::mojom::AdditionalContextPtr context) {
  Profile* profile =
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext());
  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  if (!service) {
    return;
  }

  GlicInvokeOptions invoke_options(mojom::InvocationSource::kWebDragDrop);
  invoke_options.additional_context = AdditionalTabContext(
      std::move(context), source_rfh->GetGlobalId(), PolicyCheck::kClipboard);

  // We specify the panel's Glic WebContents as the target surface.
  // TODO(b/481036078): Use GlicInstance::GetInvokeTarget here when it's
  // available.
  GlicInstance* target_instance =
      service->instance_coordinator().GetInstanceWithGlicWebContents(
          target_web_contents);

  if (target_instance) {
    invoke_options.target.conversation = target_instance->id();
  }

  if (source_tab) {
    invoke_options.target.surface = source_tab->GetHandle();
  }

  service->Invoke(std::move(invoke_options));
}

void OnReceivedTabContextForDrag(
    base::WeakPtr<content::WebContents> target_web_contents,
    content::DropData drop_data,
    content::WeakDocumentPtr source_document,
    base::expected<glic::mojom::GetContextResultPtr,
                   page_content_annotations::FetchPageContextErrorDetails>
        result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!target_web_contents) {
    return;
  }

  content::RenderFrameHost* rfh = source_document.AsRenderFrameHostIfValid();
  if (!rfh) {
    return;
  }

  content::WebContents* source_wc =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!source_wc) {
    return;
  }

  if (!result.has_value() || !result.value()->is_tab_context()) {
    return;
  }

  tabs::TabInterface* source_tab =
      tabs::TabInterface::MaybeGetFromContents(source_wc);

  auto context = BuildDragDropAdditionalContext(
      drop_data, source_tab, std::move(result.value()->get_tab_context()));

  TriggerDragDropInvoke(target_web_contents.get(), rfh, source_tab,
                        std::move(context));
}

// Restores drag-and-drop metadata (original source URL and file extensions)
// that operating systems frequently strip during drag-and-drop actions. Needed
// because OS pasteboards / clipboards are lossy channels (often stripping
// source URLs for privacy or converting image formats to generic types like
// TIFF). By matching the internally tracked drag ID to the initiating renderer,
// this recovers precise original web source properties.
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

}  // namespace

bool IsGlicWebDrag(const content::DropData& drop_data) {
  return drop_data.custom_data.contains(kGlicDragIdKey);
}

// Initiates the Glic drag-and-drop invocation workflow when a web drag is
// dropped onto the Glic panel.
//
// It executes the following steps:
// 1. Verifies the Glic drag-and-drop file upload feature flag is enabled.
// 2. Deserializes the custom Glic drag ID (`chromium/x-drag-id`) from the drop
// data.
// 3. Resolves the unique drag ID back to the originating source WebContents in
//    the same profile (guaranteeing cross-profile boundary safety).
// 4. Restores original metadata (such as the source URL and pristine file
// extension)
//    which are stripped by OS-level pasteboards.
// 5. Triggers an asynchronous `FetchPageContext` on the source tab to extract
//    the annotated page structure, title, and page text.
// 6. Invokes `OnReceivedTabContextForDrag` once page context is fetched to
//    package the payload and activate Glic.
void StartDragAndDropInvoke(content::WebContents* target_web_contents,
                            const content::DropData& drop_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(features::kGlicWebDragAndDropFileUpload)) {
    return;
  }

  // custom_data is a std::unordered_map<std::u16string, std::u16string> in
  // DropData.
  std::unordered_map<std::u16string, std::u16string>::const_iterator it =
      drop_data.custom_data.find(kGlicDragIdKey);
  if (it == drop_data.custom_data.end()) {
    return;
  }

  std::optional<base::UnguessableToken> raw_drag_id =
      base::UnguessableToken::DeserializeFromString(
          base::UTF16ToUTF8(it->second));
  if (!raw_drag_id) {
    return;
  }
  content::WebContents::DragId drag_id(*raw_drag_id);

  // Look up the originating WebContents from the drag ID.
  // Note: Passing the target's BrowserContext enforces that both the drag
  // source and drop target must belong to the same browser profile. If the drag
  // originated in a different profile, FromDragId will return nullptr,
  // preventing cross-profile data leaks.
  Profile* profile =
      Profile::FromBrowserContext(target_web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  content::WebContents* source_wc =
      content::WebContents::FromDragId(profile, drag_id);
  if (!source_wc) {
    return;
  }

  content::RenderFrameHost* rfh = source_wc->GetPrimaryMainFrame();
  if (!rfh) {
    return;
  }

  tabs::TabInterface* source_tab =
      tabs::TabInterface::MaybeGetFromContents(source_wc);
  if (!source_tab) {
    return;
  }

  content::DropData augmented_drop_data = RestoreDragMetadata(drop_data);

  auto options = mojom::GetTabContextOptions::New();
  options->max_meta_tags = 32;
  options->include_annotated_page_content = true;

  FetchPageContext(
      source_tab, *options,
      base::BindOnce(&OnReceivedTabContextForDrag,
                     target_web_contents->GetWeakPtr(), augmented_drop_data,
                     rfh->GetWeakDocumentPtr()),
      /*progress_listener=*/nullptr,
      /*is_screenshot_annotated=*/false);
}

}  // namespace glic
