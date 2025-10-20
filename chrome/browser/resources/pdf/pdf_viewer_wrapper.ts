// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
export {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
export {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
// <if expr="enable_pdf_ink2">
export {BeforeUnloadProxy, BeforeUnloadProxyImpl} from './before_unload_proxy.js';
// </if>
export {Bookmark} from './bookmark_type.js';
export {BrowserApi, ZoomBehavior} from './browser_api.js';
// <if expr="enable_pdf_ink2">
export {AnnotationBrush, AnnotationBrushType, Color, TextAlignment, TextAnnotation, TextAttributes, TextStyle, TextTypeface} from './constants.js';
// </if>
// <if expr="enable_pdf_ink2">
export {AnnotationMode} from './constants.js';
// </if>
export {Attachment, FittingType, FormFieldFocusType, Point, Rect} from './constants.js';
// <if expr="enable_pdf_save_to_drive">
export {SaveToDriveBubbleRequestType, SaveToDriveState} from './constants.js';
// </if>
export {PluginController} from './controller.js';
// <if expr="enable_pdf_ink2">
export {PluginControllerEventType} from './controller.js';
export {HIGHLIGHTER_COLORS, InkAnnotationBrushMixin, PEN_COLORS} from './elements/ink_annotation_brush_mixin.js';
export {InkAnnotationTextMixin, TEXT_COLORS, TEXT_SIZES} from './elements/ink_annotation_text_mixin.js';
export {InkBrushSelectorElement} from './elements/ink_brush_selector.js';
export {InkColorSelectorElement} from './elements/ink_color_selector.js';
export {InkSizeSelectorElement, HIGHLIGHTER_SIZES, PEN_SIZES} from './elements/ink_size_selector.js';
export {InkTextBoxElement, TextBoxState} from './elements/ink_text_box.js';
export {SelectableIconButtonElement} from './elements/selectable_icon_button.js';
export {TextAlignmentSelectorElement} from './elements/text_alignment_selector.js';
export {TextStylesSelectorElement} from './elements/text_styles_selector.js';
// </if>
export {ViewerAttachmentElement} from './elements/viewer_attachment.js';
export {ViewerAttachmentBarElement} from './elements/viewer_attachment_bar.js';
// <if expr="enable_pdf_ink2">
export {ViewerBottomToolbarElement} from './elements/viewer_bottom_toolbar.js';
export {ViewerBottomToolbarDropdownElement} from './elements/viewer_bottom_toolbar_dropdown.js';
// </if>
export {ChangePageAndXyDetail, ChangePageDetail, ChangePageOrigin, ChangeZoomDetail, NavigateDetail, ViewerBookmarkElement} from './elements/viewer_bookmark.js';
export {ViewerDocumentOutlineElement} from './elements/viewer_document_outline.js';
export {ViewerDownloadControlsElement} from './elements/viewer_download_controls.js';
export {ViewerPageSelectorElement} from './elements/viewer_page_selector.js';
export {ViewerPasswordDialogElement} from './elements/viewer_password_dialog.js';
export {ViewerPdfSidenavElement} from './elements/viewer_pdf_sidenav.js';
export {ViewerPropertiesDialogElement} from './elements/viewer_properties_dialog.js';
export {ViewerSaveControlsMixin} from './elements/viewer_save_controls_mixin.js';
// <if expr="enable_pdf_save_to_drive">
export {ViewerSaveToDriveBubbleElement} from './elements/viewer_save_to_drive_bubble.js';
export {ViewerSaveToDriveControlsElement} from './elements/viewer_save_to_drive_controls.js';
// </if>
// <if expr="enable_pdf_ink2">
export {ViewerSidePanelElement} from './elements/viewer_side_panel.js';
export {ViewerTextBottomToolbarElement} from './elements/viewer_text_bottom_toolbar.js';
// </if>
export {PAINTED_ATTRIBUTE, ViewerThumbnailElement} from './elements/viewer_thumbnail.js';
export {ViewerThumbnailBarElement} from './elements/viewer_thumbnail_bar.js';
export {ViewerToolbarElement} from './elements/viewer_toolbar.js';
export {GestureDetector, PinchEventDetail} from './gesture_detector.js';
// <if expr="enable_pdf_ink2">
export {DEFAULT_TEXTBOX_WIDTH, Ink2Manager, MIN_TEXTBOX_SIZE_PX, TextBoxInit} from './ink2_manager.js';
// </if>
export {PdfPluginElement} from './internal_plugin.js';
export {record, recordFitTo, resetForTesting, UserAction} from './metrics.js';
export {NavigatorDelegate, PdfNavigator, PdfNavigatorImpl, WindowOpenDisposition} from './navigator.js';
export {OpenPdfParamsParser, ViewMode} from './open_pdf_params_parser.js';
export {getFilenameFromURL, PdfViewerElement} from './pdf_viewer.js';
export {PdfViewerBaseElement} from './pdf_viewer_base.js';
// <if expr="enable_pdf_save_to_drive">
export {PdfViewerPrivateProxy, PdfViewerPrivateProxyImpl} from './pdf_viewer_private_proxy.js';
// </if>
// <if expr="enable_pdf_ink2">
export {hexToColor} from './pdf_viewer_utils.js';
// </if>
export {shouldIgnoreKeyEvents} from './pdf_viewer_utils.js';
// <if expr="enable_pdf_save_to_drive">
export {SaveToDriveBubbleAction, SaveToDriveBubbleState, SaveToDriveSaveType} from './save_to_drive_metrics.js';
// </if>
export {SwipeDetector, SwipeDirection} from './swipe_detector.js';
export {DocumentDimensions, LayoutOptions, PAGE_SHADOW, Viewport} from './viewport.js';
export {ZoomManager} from './zoom_manager.js';
// clang-format on
