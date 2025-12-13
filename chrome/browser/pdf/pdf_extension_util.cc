// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_extension_util.h"

#include <array>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/pdf_resources_map.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PDF_INK2)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

#if BUILDFLAG(ENABLE_PDF_INK2) || BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#include "chrome/browser/profiles/profile.h"
#endif  // BUILDFLAG(ENABLE_PDF_INK2) || BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

namespace pdf_extension_util {

namespace {

// Tags in the manifest to be replaced.
const char kNameTag[] = "<NAME>";

// Gets strings that are used both by the stand-alone PDF Viewer and the Print
// Preview PDF Viewer.
base::Value::Dict GetCommonStrings() {
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
  base::Value::Dict dict;
  for (const auto& resource : kPdfResources) {
    dict.Set(resource.name, l10n_util::GetStringUTF16(resource.id));
  }

  dict.Set("presetZoomFactors", zoom::GetPresetZoomFactorsAsJSON());
  dict.Set("pdfOopifEnabled",
           chrome_pdf::features::IsOopifPdfEnabled() ? "pdfOopifEnabled" : "");
  return dict;
}

// Gets strings that are used only by the stand-alone PDF Viewer.
base::Value::Dict GetPdfViewerStrings() {
  static constexpr webui::LocalizedString kPdfResources[] = {
      {"annotationsShowToggle", IDS_PDF_ANNOTATIONS_SHOW_TOGGLE},
      {"bookmarks", IDS_PDF_BOOKMARKS},
      {"bookmarkExpandIconAriaLabel", IDS_PDF_BOOKMARK_EXPAND_ICON_ARIA_LABEL},
      {"downloadEdited", IDS_PDF_DOWNLOAD_EDITED},
      {"downloadOriginal", IDS_PDF_DOWNLOAD_ORIGINAL},
      {"labelPageNumber", IDS_PDF_LABEL_PAGE_NUMBER},
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
      {"searchifyInProgress", IDS_PDF_SEARCHIFY_IN_PROGRESS},
      {"sidebarLabel", IDS_PDF_SIDEBAR_LABEL},
      {"thumbnailPageAriaLabel", IDS_PDF_THUMBNAIL_PAGE_ARIA_LABEL},
      {"tooltipAttachments", IDS_PDF_TOOLTIP_ATTACHMENTS},
      {"tooltipDocumentOutline", IDS_PDF_TOOLTIP_DOCUMENT_OUTLINE},
      {"tooltipDownload", IDS_PDF_TOOLTIP_DOWNLOAD},
      {"tooltipDownloadAttachment", IDS_PDF_TOOLTIP_DOWNLOAD_ATTACHMENT},
      {"tooltipPrint", IDS_PDF_TOOLTIP_PRINT},
      {"tooltipRotateCCW", IDS_PDF_TOOLTIP_ROTATE_CCW},
      {"tooltipThumbnails", IDS_PDF_TOOLTIP_THUMBNAILS},
      {"zoomTextInputAriaLabel", IDS_PDF_ZOOM_TEXT_INPUT_ARIA_LABEL},
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
      {"saveToDriveDialogCancelUploadButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_CANCEL_UPLOAD_BUTTON_LABEL},
      {"saveToDriveDialogConnectionErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_CONNECTION_ERROR_MESSAGE},
      {"saveToDriveDialogErrorTitle", IDS_SAVE_TO_DRIVE_DIALOG_ERROR_TITLE},
      {"saveToDriveDialogManageStorageButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_MANAGE_STORAGE_BUTTON_LABEL},
      {"saveToDriveDialogOpenInDriveButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_OPEN_IN_DRIVE_BUTTON_LABEL},
      {"saveToDriveDialogRetryButtonLabel",
       IDS_SAVE_TO_DRIVE_DIALOG_RETRY_BUTTON_LABEL},
      {"saveToDriveDialogSessionTimeoutErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_SESSION_TIMEOUT_ERROR_MESSAGE},
      {"saveToDriveDialogStorageFullErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_STORAGE_FULL_ERROR_MESSAGE},
      {"saveToDriveDialogSuccessMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_SUCCESS_MESSAGE},
      {"saveToDriveDialogSuccessTitle", IDS_SAVE_TO_DRIVE_DIALOG_SUCCESS_TITLE},
      {"saveToDriveDialogUnknownErrorMessage",
       IDS_SAVE_TO_DRIVE_DIALOG_UNKNOWN_ERROR_MESSAGE},
      {"saveToDriveDialogUploadingTitle",
       IDS_SAVE_TO_DRIVE_DIALOG_UPLOADING_TITLE},
      {"tooltipSaveToDrive", IDS_PDF_TOOLTIP_SAVE_TO_DRIVE},
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#if BUILDFLAG(ENABLE_PDF_INK2)
      {"cancelButton", IDS_CANCEL},
      {"annotationPen", IDS_PDF_ANNOTATION_PEN},
      {"annotationHighlighter", IDS_PDF_ANNOTATION_HIGHLIGHTER},
      {"annotationEraser", IDS_PDF_ANNOTATION_ERASER},
      {"annotationUndo", IDS_PDF_ANNOTATION_UNDO},
      {"annotationRedo", IDS_PDF_ANNOTATION_REDO},
      {"annotationColorBlack", IDS_PDF_ANNOTATION_COLOR_BLACK},
      {"annotationColorRed", IDS_PDF_ANNOTATION_COLOR_RED},
      {"annotationColorYellow", IDS_PDF_ANNOTATION_COLOR_YELLOW},
      {"annotationColorGreen", IDS_PDF_ANNOTATION_COLOR_GREEN},
      {"annotationColorWhite", IDS_PDF_ANNOTATION_COLOR_WHITE},
      {"annotationColorOrange", IDS_PDF_ANNOTATION_COLOR_ORANGE},
      {"annotationColorBlue", IDS_PDF_ANNOTATION_COLOR_BLUE},
      {"annotationColorLightGrey", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREY},
      {"annotationColorLightOrange", IDS_PDF_ANNOTATION_COLOR_LIGHT_ORANGE},
      {"annotationColorLightGreen", IDS_PDF_ANNOTATION_COLOR_LIGHT_GREEN},
      {"annotationColorLightBlue", IDS_PDF_ANNOTATION_COLOR_LIGHT_BLUE},
      {"ink2Draw", IDS_PDF_INK2_DRAW},
      {"ink2Tool", IDS_PDF_INK2_ANNOTATION_TOOL},
      {"ink2Size", IDS_PDF_INK2_ANNOTATION_SIZE},
      {"ink2Color", IDS_PDF_INK2_ANNOTATION_COLOR},
      {"ink2BrushSizeExtraThin", IDS_PDF_INK2_ANNOTATION_SIZE_EXTRA_THIN},
      {"ink2BrushSizeThin", IDS_PDF_INK2_ANNOTATION_SIZE_THIN},
      {"ink2BrushSizeMedium", IDS_PDF_INK2_ANNOTATION_SIZE_MEDIUM},
      {"ink2BrushSizeThick", IDS_PDF_INK2_ANNOTATION_SIZE_THICK},
      {"ink2BrushSizeExtraThick", IDS_PDF_INK2_ANNOTATION_SIZE_EXTRA_THICK},
      {"ink2BrushColorLightRed", IDS_PDF_INK2_ANNOTATION_COLOR_LIGHT_RED},
      {"ink2BrushColorLightYellow", IDS_PDF_INK2_ANNOTATION_COLOR_LIGHT_YELLOW},
      {"ink2BrushColorDarkGrey1", IDS_PDF_INK2_ANNOTATION_COLOR_DARK_GREY_1},
      {"ink2BrushColorDarkGrey2", IDS_PDF_INK2_ANNOTATION_COLOR_DARK_GREY_2},
      {"ink2BrushColorRed1", IDS_PDF_INK2_ANNOTATION_COLOR_RED_1},
      {"ink2BrushColorYellow1", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_1},
      {"ink2BrushColorGreen1", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_1},
      {"ink2BrushColorBlue1", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_1},
      {"ink2BrushColorTan1", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_1},
      {"ink2BrushColorRed2", IDS_PDF_INK2_ANNOTATION_COLOR_RED_2},
      {"ink2BrushColorYellow2", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_2},
      {"ink2BrushColorGreen2", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_2},
      {"ink2BrushColorBlue2", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_2},
      {"ink2BrushColorTan2", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_2},
      {"ink2BrushColorRed3", IDS_PDF_INK2_ANNOTATION_COLOR_RED_3},
      {"ink2BrushColorYellow3", IDS_PDF_INK2_ANNOTATION_COLOR_YELLOW_3},
      {"ink2BrushColorGreen3", IDS_PDF_INK2_ANNOTATION_COLOR_GREEN_3},
      {"ink2BrushColorBlue3", IDS_PDF_INK2_ANNOTATION_COLOR_BLUE_3},
      {"ink2BrushColorTan3", IDS_PDF_INK2_ANNOTATION_COLOR_TAN_3},
      {"ink2TextAnnotation", IDS_PDF_INK2_TEXT_ANNOTATION},
      {"ink2TextFont", IDS_PDF_INK2_TEXT_FONT},
      {"ink2TextFontSansSerif", IDS_PDF_INK2_TEXT_FONT_SANS_SERIF},
      {"ink2TextFontSerif", IDS_PDF_INK2_TEXT_FONT_SERIF},
      {"ink2TextFontMonospace", IDS_PDF_INK2_TEXT_FONT_MONOSPACE},
      {"ink2TextFontSize", IDS_PDF_INK2_TEXT_FONT_SIZE},
      {"ink2TextStyles", IDS_PDF_INK2_TEXT_STYLES},
      {"ink2TextStyleBold", IDS_PDF_INK2_TEXT_STYLE_BOLD},
      {"ink2TextStyleItalic", IDS_PDF_INK2_TEXT_STYLE_ITALIC},
      {"ink2TextAlignment", IDS_PDF_INK2_TEXT_ALIGNMENT},
      {"ink2TextAlignLeft", IDS_PDF_INK2_TEXT_ALIGN_LEFT},
      {"ink2TextAlignCenter", IDS_PDF_INK2_TEXT_ALIGN_CENTER},
      {"ink2TextAlignRight", IDS_PDF_INK2_TEXT_ALIGN_RIGHT},
      {"ink2TextColor", IDS_PDF_INK2_TEXT_COLOR},
      {"ink2TextColorCyan1", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_1},
      {"ink2TextColorCyan2", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_2},
      {"ink2TextColorCyan3", IDS_PDF_INK2_ANNOTATION_COLOR_CYAN_3},
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
  };
  base::Value::Dict dict;
  for (const auto& resource : kPdfResources) {
    dict.Set(resource.name, l10n_util::GetStringUTF16(resource.id));
  }

#if BUILDFLAG(ENABLE_PDF_INK2)
  std::u16string edit_string = l10n_util::GetStringUTF16(IDS_EDIT);
  std::erase(edit_string, '&');
  dict.Set("editButton", edit_string);
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

  webui::SetLoadTimeDataDefaults(g_browser_process->GetApplicationLocale(),
                                 &dict);
  return dict;
}

bool IsPrintingEnabled(content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS)
  return ash::IsUserBrowserContext(context);
#else
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(ENABLE_PDF_INK2)
bool IsPdfAnnotationsEnabledByPolicy(content::BrowserContext* context) {
  PrefService* prefs =
      context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
  return !prefs || !prefs->IsManagedPreference(prefs::kPdfAnnotationsEnabled) ||
         prefs->GetBoolean(prefs::kPdfAnnotationsEnabled);
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

#if BUILDFLAG(ENABLE_PDF_INK2)
bool IsPdfInk2AnnotationsEnabled(content::BrowserContext* context) {
  return base::FeatureList::IsEnabled(chrome_pdf::features::kPdfInk2) &&
         IsPdfAnnotationsEnabledByPolicy(context);
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

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

base::Value::Dict GetStrings(PdfViewerContext context) {
  base::Value::Dict dict = GetCommonStrings();
  if (context == PdfViewerContext::kPdfViewer ||
      context == PdfViewerContext::kAll) {
    dict.Merge(GetPdfViewerStrings());
  }
  if (context == PdfViewerContext::kPrintPreview ||
      context == PdfViewerContext::kAll) {
    // Nothing to do yet, since there are no PrintPreview-only strings.
  }
  return dict;
}

base::Value::Dict GetAdditionalData(content::BrowserContext* context) {
  // NOTE: This function should not include any data used for $i18n{}
  // replacements. The i18n string resources should be added using GetStrings()
  // above instead.
  base::Value::Dict dict;
  dict.Set("printingEnabled", IsPrintingEnabled(context));

#if BUILDFLAG(ENABLE_PDF_INK2)
  const bool use_ink2 = IsPdfInk2AnnotationsEnabled(context);
  dict.Set("pdfInk2Enabled", use_ink2);
  dict.Set("pdfTextAnnotationsEnabled",
           use_ink2 && chrome_pdf::features::kPdfInk2TextAnnotations.Get());
#endif  // BUILDFLAG(ENABLE_PDF_INK2)
  dict.Set("pdfGetSaveDataInBlocks",
           base::FeatureList::IsEnabled(
               chrome_pdf::features::kPdfGetSaveDataInBlocks));
  dict.Set("pdfUseShowSaveFilePicker",
           base::FeatureList::IsEnabled(
               chrome_pdf::features::kPdfUseShowSaveFilePicker));
  dict.Set(
      "pdfSearchifySaveEnabled",
      base::FeatureList::IsEnabled(chrome_pdf::features::kPdfSearchifySave));

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  const bool save_to_drive_enabled =
      base::FeatureList::IsEnabled(chrome_pdf::features::kPdfSaveToDrive) &&
      !Profile::FromBrowserContext(context)->IsOffTheRecord();
  dict.Set("pdfSaveToDrive", save_to_drive_enabled);
  dict.Set("pdfSaveToDriveHelpCenterURL",
           chrome::kPdfViewerSaveToDriveHelpCenterURL);
#endif
  return dict;
}

std::vector<webui::ResourcePath> GetResources(PdfViewerContext context) {
  static constexpr auto kExcludeFromPdfViewer =
      std::to_array<std::string_view>({
          "pdf/index_print.html",
          "pdf/main_print.js",
          "pdf/pdf_print_wrapper.js",
      });
  static constexpr auto kExcludeFromPrintPreview =
      std::to_array<std::string_view>({
          "pdf/index.html",
          "pdf/main.js",
          "pdf/pdf_viewer_wrapper.js",
      });
  base::span<const std::string_view> exclusions;

  switch (context) {
    case PdfViewerContext::kPdfViewer:
      exclusions = kExcludeFromPdfViewer;
      break;
    case PdfViewerContext::kPrintPreview:
      exclusions = kExcludeFromPrintPreview;
      break;
    default:
      NOTREACHED();
  }

  std::vector<webui::ResourcePath> resources;
  resources.reserve(std::size(kPdfResources));
  for (const webui::ResourcePath& resource : kPdfResources) {
    if (base::Contains(exclusions, resource.path)) {
      continue;
    }
    resources.push_back(resource);
  }
  return resources;
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

void DispatchShouldUpdateViewportEvent(content::RenderFrameHost* embedder_host,
                                       const GURL& new_pdf_url) {
  base::Value::List args;
  args.Append(new_pdf_url.spec());

  content::BrowserContext* context = embedder_host->GetBrowserContext();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::PDF_VIEWER_PRIVATE_ON_SHOULD_UPDATE_VIEWPORT,
      extensions::api::pdf_viewer_private::OnShouldUpdateViewport::kEventName,
      std::move(args), context);
  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  event_router->DispatchEventToExtension(extension_misc::kPdfExtensionId,
                                         std::move(event));
}

}  // namespace pdf_extension_util
