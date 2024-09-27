// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_util.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/zoom/page_zoom_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler_private.h"
#include "pdf/buildflags.h"
#include "pdf/pdf_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace pdf_extension_util {

namespace {

// Tags in the manifest to be replaced.
const char kNameTag[] = "<NAME>";

// Adds strings that are used both by the stand-alone PDF Viewer and the Print
// Preview PDF Viewer.
void AddCommonStrings(base::Value::Dict* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"errorDialogTitle", IDS_PDF_ERROR_DIALOG_TITLE},
      {"pageLoadFailed", IDS_PDF_PAGE_LOAD_FAILED},
      {"pageLoading", IDS_PDF_PAGE_LOADING},
      {"pageReload", IDS_PDF_PAGE_RELOAD_BUTTON},
      {"tooltipFitToPage", IDS_PDF_TOOLTIP_FIT_PAGE},
      {"tooltipFitToWidth", IDS_PDF_TOOLTIP_FIT_WIDTH},
      {"tooltipZoomIn", IDS_PDF_TOOLTIP_ZOOM_IN},
      {"tooltipZoomOut", IDS_PDF_TOOLTIP_ZOOM_OUT},
      {"twoUpViewEnable", IDS_PDF_TWO_UP_VIEW_ENABLE},
  };
  for (const auto& resource : kPdfResources)
    dict->Set(resource.name, l10n_util::GetStringUTF16(resource.id));

  dict->Set("presetZoomFactors", zoom::GetPresetZoomFactorsAsJSON());
  dict->Set("pdfCr23Enabled",
            base::FeatureList::IsEnabled(chrome_pdf::features::kPdfCr23)
                ? "pdfCr23Enabled"
                : "");
  dict->Set("pdfOopifEnabled",
            chrome_pdf::features::IsOopifPdfEnabled() ? "pdfOopifEnabled" : "");
}

// Adds strings that are used only by the stand-alone PDF Viewer.
void AddPdfViewerStrings(base::Value::Dict* dict) {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"annotationsShowToggle", IDS_PDF_ANNOTATIONS_SHOW_TOGGLE},
      {"bookmarks", IDS_PDF_BOOKMARKS},
      {"bookmarkExpandIconAriaLabel", IDS_PDF_BOOKMARK_EXPAND_ICON_ARIA_LABEL},
      {"downloadEdited", IDS_PDF_DOWNLOAD_EDITED},
      {"downloadOriginal", IDS_PDF_DOWNLOAD_ORIGINAL},
      {"labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER},
      {"menu", IDS_MENU},
      {"moreActions", IDS_DOWNLOAD_MORE_ACTIONS},
      {"oversizeAttachmentWarning", IDS_PDF_OVERSIZE_ATTACHMENT_WARNING},
      {"passwordDialogTitle", IDS_PDF_PASSWORD_DIALOG_TITLE},
      {"passwordInvalid", IDS_PDF_PASSWORD_INVALID},
      {"passwordPrompt", IDS_PDF_NEED_PASSWORD},
      {"passwordSubmit", IDS_PDF_PASSWORD_SUBMIT},
      {"present", IDS_PDF_PRESENT},
      {"propertiesApplication", IDS_PDF_PROPERTIES_APPLICATION},
      {"propertiesAuthor", IDS_PDF_PROPERTIES_AUTHOR},
      {"propertiesCreated", IDS_PDF_PROPERTIES_CREATED},
      {"propertiesDialogClose", IDS_CLOSE},
      {"propertiesDialogTitle", IDS_PDF_PROPERTIES_DIALOG_TITLE},
      {"propertiesFastWebView", IDS_PDF_PROPERTIES_FAST_WEB_VIEW},
      {"propertiesFastWebViewNo", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_NO},
      {"propertiesFastWebViewYes", IDS_PDF_PROPERTIES_FAST_WEB_VIEW_YES},
      {"propertiesFileName", IDS_PDF_PROPERTIES_FILE_NAME},
      {"propertiesFileSize", IDS_PDF_PROPERTIES_FILE_SIZE},
      {"propertiesKeywords", IDS_PDF_PROPERTIES_KEYWORDS},
      {"propertiesModified", IDS_PDF_PROPERTIES_MODIFIED},
      {"propertiesPageCount", IDS_PDF_PROPERTIES_PAGE_COUNT},
      {"propertiesPageSize", IDS_PDF_PROPERTIES_PAGE_SIZE},
      {"propertiesPdfProducer", IDS_PDF_PROPERTIES_PDF_PRODUCER},
      {"propertiesPdfVersion", IDS_PDF_PROPERTIES_PDF_VERSION},
      {"propertiesSubject", IDS_PDF_PROPERTIES_SUBJECT},
      {"propertiesTitle", IDS_PDF_PROPERTIES_TITLE},
      {"rotationStateLabel0", IDS_PDF_ROTATION_STATE_LABEL_0},
      {"rotationStateLabel90", IDS_PDF_ROTATION_STATE_LABEL_90},
      {"rotationStateLabel180", IDS_PDF_ROTATION_STATE_LABEL_180},
      {"rotationStateLabel270", IDS_PDF_ROTATION_STATE_LABEL_270},
      {"thumbnailPageAriaLabel", IDS_PDF_THUMBNAIL_PAGE_ARIA_LABEL},
      {"tooltipAttachments", IDS_PDF_TOOLTIP_ATTACHMENTS},
      {"tooltipDocumentOutline", IDS_PDF_TOOLTIP_DOCUMENT_OUTLINE},
      {"tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD},
      {"tooltipDownloadAttachment", IDS_PDF_TOOLTIP_DOWNLOAD_ATTACHMENT},
      {"tooltipPrint", IDS_PDF_TOOLTIP_PRINT},
      {"tooltipRotateCCW", IDS_PDF_TOOLTIP_ROTATE_CCW},
      {"tooltipThumbnails", IDS_PDF_TOOLTIP_THUMBNAILS},
      {"zoomTextInputAriaLabel", IDS_PDF_ZOOM_TEXT_INPUT_ARIA_LABEL},
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_PDF_INK2)
      {"tooltipAnnotate", IDS_PDF_ANNOTATION_ANNOTATE},
      {"annotationDocumentTooLarge", IDS_PDF_ANNOTATION_DOCUMENT_TOO_LARGE},
      {"annotationDocumentProtected", IDS_PDF_ANNOTATION_DOCUMENT_PROTECTED},
      {"annotationDocumentRotated", IDS_PDF_ANNOTATION_DOCUMENT_ROTATED},
      {"annotationEditInDefaultView", IDS_PDF_ANNOTATION_EDIT_IN_DEFAULT_VIEW},
      {"annotationResetRotate", IDS_PDF_ANNOTATION_RESET_ROTATE},
      {"annotationResetTwoPageView", IDS_PDF_ANNOTATION_RESET_TWO_PAGE_VIEW},
      {"annotationResetRotateAndTwoPageView",
       IDS_PDF_ANNOTATION_RESET_ROTATE_AND_TWO_PAGE_VIEW},
      {"cancelButton", IDS_CANCEL},
      {"annotationPen", IDS_PDF_ANNOTATION_PEN},
      {"annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER},
      {"annotationEraser", IDS_PDF_ANNOTATION_ERASER},
      {"annotationUndo", IDS_PDF_ANNOTATION_UNDO},
      {"annotationRedo", IDS_PDF_ANNOTATION_REDO},
      {"annotationExpand", IDS_PDF_ANNOTATION_EXPAND},
      {"annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK},
      {"annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED},
      {"annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW},
      {"annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN},
      {"annotationColorCyan", IDS_PDF_ANNOTATION_COLOR_CYAN},
      {"annotationColorPurple", IDS_PDF_ANNOTATION_COLOR_PURPLE},
      {"annotationColorBrown", IDS_PDF_ANNOTATION_COLOR_BROWN},
      {"annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE},
      {"annotationColorCrimson", IDS_PDF_ANNOTATION_COLOR_CRIMSON},
      {"annotationColorAmber", IDS_PDF_ANNOTATION_COLOR_AMBER},
      {"annotationColorAvocadoGreen", IDS_PDF_ANNOTATION_COLOR_AVOCADO_GREEN},
      {"annotationColorCobaltBlue", IDS_PDF_ANNOTATION_COLOR_COBALT_BLUE},
      {"annotationColorDeepPurple", IDS_PDF_ANNOTATION_COLOR_DEEP_PURPLE},
      {"annotationColorDarkBrown", IDS_PDF_ANNOTATION_COLOR_DARK_BROWN},
      {"annotationColorDarkGrey", IDS_PDF_ANNOTATION_COLOR_DARK_GREY},
      {"annotationColorHotPink", IDS_PDF_ANNOTATION_COLOR_HOT_PINK},
      {"annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE},
      {"annotationColorLime", IDS_PDF_ANNOTATION_COLOR_LIME},
      {"annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE},
      {"annotationColorViolet", IDS_PDF_ANNOTATION_COLOR_VIOLET},
      {"annotationColorTeal", IDS_PDF_ANNOTATION_COLOR_TEAL},
      {"annotationColorLightGrey", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY},
      {"annotationColorLightPink", IDS_PDF_ANNOTATION_COLOR_LIGHT_PINK},
      {"annotationColorLightOrange", IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE},
      {"annotationColorLightGreen", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN},
      {"annotationColorLightBlue", IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE},
      {"annotationColorLavender", IDS_PDF_ANNOTATION_COLOR_LAVENDER},
      {"annotationColorLightTeal", IDS_PDF_ANNOTATION_COLOR_LIGHT_TEAL},
      {"annotationSize1", IDS_PDF_ANNOTATION_SIZE1},
      {"annotationSize2", IDS_PDF_ANNOTATION_SIZE2},
      {"annotationSize3", IDS_PDF_ANNOTATION_SIZE3},
      {"annotationSize4", IDS_PDF_ANNOTATION_SIZE4},
      {"annotationSize8", IDS_PDF_ANNOTATION_SIZE8},
      {"annotationSize12", IDS_PDF_ANNOTATION_SIZE12},
      {"annotationSize16", IDS_PDF_ANNOTATION_SIZE16},
      {"annotationSize20", IDS_PDF_ANNOTATION_SIZE20},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_PDF_INK2)
  };
  for (const auto& resource : kPdfResources)
    dict->Set(resource.name, l10n_util::GetStringUTF16(resource.id));

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_PDF_INK2)
  std::u16string edit_string = l10n_util::GetStringUTF16(IDS_EDIT);
  std::erase(edit_string, '&');
  dict->Set("editButton", edit_string);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(ENABLE_PDF_INK2)

  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 dict);
}

}  // namespace

std::string GetManifest() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static constexpr char kExtensionName[] = "Chrome PDF Viewer";
#else
  static constexpr char kExtensionName[] = "Chromium PDF Viewer";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  std::string manifest_contents(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PDF_MANIFEST));
  DCHECK(manifest_contents.find(kNameTag) != std::string::npos);
  base::ReplaceFirstSubstringAfterOffset(&manifest_contents, 0, kNameTag,
                                         kExtensionName);

  return manifest_contents;
}

void AddStrings(PdfViewerContext context, base::Value::Dict* dict) {
  AddCommonStrings(dict);
  if (context == PdfViewerContext::kPdfViewer ||
      context == PdfViewerContext::kAll) {
    AddPdfViewerStrings(dict);
  }
  if (context == PdfViewerContext::kPrintPreview ||
      context == PdfViewerContext::kAll) {
    // Nothing to do yet, since there are no PrintPreview-only strings.
  }
}

void AddAdditionalData(bool enable_printing,
                       bool enable_annotations,
                       base::Value::Dict* dict) {
  // NOTE: This function should not include any data used for $i18n{}
  // replacements. The i18n string resources should be added using AddStrings()
  // above instead.
  bool printing_enabled = true;
  bool annotations_enabled = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  printing_enabled = enable_printing;
  annotations_enabled = enable_annotations;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(ENABLE_PDF_INK2)
  bool use_ink2 = base::FeatureList::IsEnabled(chrome_pdf::features::kPdfInk2);
  if (use_ink2) {
    annotations_enabled = enable_annotations;
  }
  dict->Set("pdfInk2Enabled", use_ink2);
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
  dict->Set("printingEnabled", printing_enabled);
  dict->Set("pdfAnnotationsEnabled", annotations_enabled);
}

bool MaybeDispatchSaveEvent(content::RenderFrameHost* embedder_host) {
  CHECK(chrome_pdf::features::IsOopifPdfEnabled());

  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromRenderFrameHost(embedder_host);
  if (!pdf_viewer_stream_manager) {
    return false;
  }

  // Continue only if the PDF plugin should handle the save event.
  if (!pdf_viewer_stream_manager->PluginCanSave(embedder_host)) {
    return false;
  }

  base::WeakPtr<extensions::StreamContainer> stream =
      pdf_viewer_stream_manager->GetStreamContainer(embedder_host);

  base::Value::List args;
  args.Append(stream->stream_url().spec());

  content::BrowserContext* context = embedder_host->GetBrowserContext();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::PDF_VIEWER_PRIVATE_ON_SAVE,
      extensions::api::pdf_viewer_private::OnSave::kEventName, std::move(args),
      context);
  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  event_router->DispatchEventToExtension(extension_misc::kPdfExtensionId,
                                         std::move(event));
  return true;
}

}  // namespace pdf_extension_util
