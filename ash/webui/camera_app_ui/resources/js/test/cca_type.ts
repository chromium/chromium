// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExpertOption} from '../expert.js';
import {StateUnion} from '../state.js';
import {ViewName} from '../type.js';

export const SELECTOR_MAP = {
  backAspectRatioOptions: `#view-photo-aspect-ratio-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  backPhotoResolutionOptions: `#view-photo-resolution-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  backVideoResolutionOptions: `#view-video-resolution-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  barcodeChipText: '.barcode-chip-text',
  barcodeChipURL: '.barcode-chip-url a',
  barcodeCopyTextButton: '#barcode-chip-text-container .barcode-copy-button',
  barcodeCopyURLButton: '#barcode-chip-url-container .barcode-copy-button',
  bitrateMultiplierRangeInput: '#bitrate-slider input[type=range]',
  cancelResultButton: 'button[i18n-label=cancel_review_button]',
  confirmResultButton: 'button[i18n-label=confirm_review_button]',
  documentAddPageButton:
      '.document-preview-mode button[i18n-label=add_new_page_button]',
  documentBackButton: '#back-to-review-document',
  documentCancelButton:
      '.document-preview-mode button[i18n-text=cancel_review_button]',
  documentCornerOverlay: '#preview-document-corner-overlay',
  documentDialogButton: `#view-document-mode-dialog ` +
      `button[i18n-text=document_mode_dialog_got_it]`,
  documentDoneFixButton: `.document-fix-mode ` +
      `button[i18n-text=label_crop_done]`,
  documentFixButton:
      '.document-preview-mode button[i18n-label=fix_page_button]',
  documentFixModeCorner: '.document-fix-mode .dot',
  documentFixModeImage: '.document-fix-mode .image',
  documentPreviewModeImage: '.document-preview-mode .image',
  documentReview: '#view-document-review',
  documentSaveAsPhotoButton:
      '.document-preview-mode button[i18n-text=label_save_photo_document]',
  documentSaveAsPdfButton:
      '.document-preview-mode button[i18n-text=label_save_pdf_document]',
  expertCustomVideoParametersOption: '#custom-video-parameters',
  expertModeButton: '#settings-expert',
  expertModeOption: '#expert-enable-expert-mode',
  expertMultiStreamRecordingOption: '#expert-enable-multistream-recording',
  expertSaveMetadataOption: '#expert-save-metadata',
  expertShowMetadataOption: '#expert-show-metadata',
  feedbackButton: '#settings-feedback',
  frontAspectRatioOptions:
      '#view-photo-aspect-ratio-settings .menu-item>input[data-facing="user"]',
  frontPhotoResolutionOptions:
      '#view-photo-resolution-settings .menu-item>input[data-facing="user"]',
  frontVideoResolutionOptions:
      '#view-video-resolution-settings .menu-item>input[data-facing="user"]',
  galleryButtonCover: '#gallery-enter>img',
  galleryEnter: '#gallery-enter',
  gifRecordingOption: 'input[type=radio][data-state=record-type-gif]',
  gifReviewRetakeButton: '#review-retake',
  gifReviewSaveButton: '#view-review button[i18n-text=label_save]',
  helpButton: '#settings-help',
  lowStorageDialog: '#view-low-storage-dialog',
  lowStorageDialogManageButton:
      '#view-low-storage-dialog button.dialog-negative-button',
  lowStorageDialogOKButton:
      '#view-low-storage-dialog button.dialog-positive-button',
  lowStorageWarning: '#nudge',
  modeSelector: '#modes-group',
  openGridPanelButton: '#open-grid-panel',
  openMirrorPanelButton: '#open-mirror-panel',
  openPTZPanelButton: '#open-ptz-panel',
  openTimerPanelButton: '#open-timer-panel',
  optionsContainer: '#options-container',
  panLeftButton: '#pan-left',
  panRightButton: '#pan-right',
  photoAspectRatioSettingButton: '#settings-photo-aspect-ratio',
  photoResolutionSettingButton: '#settings-photo-resolution',
  previewVideo: '#preview-video',
  previewViewport: '#preview-viewport',
  ptzResetAllButton: '#ptz-reset-all',
  reviewView: '#view-review',
  scanBarcodeOption: '#scan-barcode',
  scanDocumentModeOption: '#scan-document',
  scanModeButton: '.mode-item>input[data-mode="scan"]',
  settingsButton: '#open-settings',
  shutter: '.shutter',
  switchDeviceButton: '#switch-device',
  tiltDownButton: '#tilt-down',
  tiltUpButton: '#tilt-up',
  videoPauseResumeButton: '#pause-recordvideo',
  videoProfileSelect: '#video-profile',
  videoResolutionSettingButton: '#settings-video-resolution',
  videoSnapshotButton: '#video-snapshot',
  zoomInButton: '#zoom-in',
  zoomOutButton: '#zoom-out',
} as const;
export type UIComponent = keyof typeof SELECTOR_MAP;

export const SETTING_OPTION_MAP = {
  customVideoParametersOption: {
    component: 'expertCustomVideoParametersOption',
    state: ExpertOption.CUSTOM_VIDEO_PARAMETERS,
  },
  expertModeOption: {
    component: 'expertModeOption',
    state: ExpertOption.EXPERT,
  },
  multiStreamRecordingOption: {
    component: 'expertMultiStreamRecordingOption',
    state: ExpertOption.ENABLE_MULTISTREAM_RECORDING,
  },
  saveMetadataOption: {
    component: 'expertSaveMetadataOption',
    state: ExpertOption.SAVE_METADATA,
  },
  showMetadataOption: {
    component: 'expertShowMetadataOption',
    state: ExpertOption.SHOW_METADATA,
  },
} satisfies Record<string, {component: UIComponent, state: StateUnion}>;
export type SettingOption = keyof typeof SETTING_OPTION_MAP;

export const SETTING_MENU_MAP = {
  expertMenu: {
    component: 'expertModeButton',
    view: ViewName.EXPERT_SETTINGS,
  },
  mainMenu: {
    component: 'settingsButton',
    view: ViewName.SETTINGS,
  },
  photoAspectRatioMenu: {
    component: 'photoAspectRatioSettingButton',
    view: ViewName.PHOTO_ASPECT_RATIO_SETTINGS,
  },
  photoResolutionMenu: {
    component: 'photoResolutionSettingButton',
    view: ViewName.PHOTO_RESOLUTION_SETTINGS,
  },
  videoResolutionMenu: {
    component: 'videoResolutionSettingButton',
    view: ViewName.VIDEO_RESOLUTION_SETTINGS,
  },
} satisfies Record<string, {component: UIComponent, view: ViewName}>;
export type SettingMenu = keyof typeof SETTING_MENU_MAP;
