// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExpertOption} from '../expert.js';
import {State, StateUnion} from '../state.js';
import {ViewName} from '../type.js';

export const SELECTOR_MAP = {
  backAspectRatioOptions: `#view-photo-aspect-ratio-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  backPhotoResolutionOptions: `#view-photo-resolution-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  backVideoResolutionOptions: `#view-video-resolution-settings ` +
      `.menu-item>input[data-facing="environment"]`,
  barcodeChipText: '.barcode-chip-text',
  // This is used by tast side and name needs to be keep as is for backward
  // compatibility.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  barcodeChipURL: '#barcode-chip-url',
  barcodeChipWifi: '#barcode-chip-wifi',
  barcodeCopyTextButton: '#barcode-chip-text-container .barcode-copy-button',
  // This is used by tast side and name needs to be keep as is for backward
  // compatibility.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  barcodeCopyURLButton: '#barcode-chip-url-container .barcode-copy-button',
  bitrateMultiplierRangeInput: '#bitrate-slider input[type=range]',
  cancelResultButton: 'button[i18n-label=cancel_review_button]',
  confirmResultButton: 'button[i18n-label=confirm_review_button]',
  documentAddPageButton:
      '.document-preview-mode button[i18n-label=add_new_page_button]',
  documentBackButton: '#back-to-review-document',
  documentCancelButton:
      '.document-preview-mode button[i18n-text=cancel_review_button]',
  documentCorner: '#preview-document-corner-overlay .corner',
  documentDoneFixButton: `.document-fix-mode ` +
      `button[i18n-text=label_crop_done]`,
  documentFixButton:
      '.document-preview-mode button[i18n-label=fix_page_button]',
  documentFixModeCorner: '.document-fix-mode .dot',
  documentFixModeImage: '.document-fix-mode .image',
  documentPreviewModeImage: '.document-preview-mode .image',
  documentReview: '#view-document-review',
  documentSaveAsPdfButton:
      '.document-preview-mode button[i18n-text=label_save_pdf_document]',
  documentSaveAsPhotoButton:
      '.document-preview-mode button[i18n-text=label_save_photo_document]',
  expertCustomVideoParametersOption: '#custom-video-parameters',
  expertModeButton: '#settings-expert',
  expertModeOption: '#expert-enable-expert-mode',
  expertSaveMetadataOption: '#expert-save-metadata',
  expertShowMetadataOption: '#expert-show-metadata',
  feedbackButton: '#settings-feedback',
  fps60Buttons: `.fps-60:not(.invisible)`,
  frontAspectRatioOptions:
      '#view-photo-aspect-ratio-settings .menu-item>input[data-facing="user"]',
  frontPhotoResolutionOptions:
      '#view-photo-resolution-settings .menu-item>input[data-facing="user"]',
  frontVideoResolutionOptions:
      '#view-video-resolution-settings .menu-item>input[data-facing="user"]',
  galleryButton: 'gallery-button',
  gifRecordingOption: 'input[type=radio][data-state=record-type-gif]',
  gifReviewRetakeButton: '#review-retake',
  gifReviewSaveButton: '#view-review button[i18n-text=label_save]',
  gridOptionGoldenRatio: 'span[i18n-aria=label_grid_golden]',
  helpButton: '#settings-help',
  lowStorageDialog: '#view-low-storage-dialog',
  lowStorageDialogManageButton:
      '#view-low-storage-dialog button.dialog-negative-button',
  // This is used by tast side and name needs to be keep as is for backward
  // compatibility.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  lowStorageDialogOKButton:
      '#view-low-storage-dialog button.dialog-positive-button',
  lowStorageWarning: '#nudge',
  mirrorOptionOff: 'span[i18n-aria=aria_mirror_off]',
  mirrorOptionOn: 'span[i18n-aria=aria_mirror_on]',
  modeSelector: 'mode-selector',
  openGridPanelButton: '#open-grid-panel',
  openMirrorPanelButton: '#open-mirror-panel',
  // This is used by tast side and name needs to be keep as is for backward
  // compatibility.
  // eslint-disable-next-line @typescript-eslint/naming-convention
  openPTZPanelButton: '#open-ptz-panel',
  openTimerPanelButton: '#open-timer-panel',
  panLeftButton: '#pan-left',
  panRightButton: '#pan-right',
  photoAspectRatioSettingButton: '#settings-photo-aspect-ratio',
  photoResolutionSettingButton: '#settings-photo-resolution',
  // TODO(kamchonlathorn): Remove this once its usage in Tast is removed.
  previewExposureTime: '#preview-exposure-time',
  previewOcrOption: '#settings-preview-ocr',
  previewResolution: '#preview-resolution',
  previewVideo: '#preview-video',
  previewViewport: '#preview-viewport',
  ptzResetAllButton: '#ptz-reset-all',
  reviewView: '#view-review',
  scanBarcodeOption: '#scan-barcode',
  scanDocumentModeOption: '#scan-document',
  settingsButton: '#open-settings',
  settingsButtonContainer: 'div:has(> #open-settings)',
  settingsHeader: '#settings-header',
  shutter: '.shutter',
  switchDeviceButton: 'switch-device-button',
  snackbar: '.snackbar',
  tiltDownButton: '#tilt-down',
  tiltUpButton: '#tilt-up',
  timeLapseRecordingOption:
      'input[type=radio][data-state=record-type-time-lapse]',
  timerOption10Seconds: 'span[i18n-aria=aria_timer_10s]',
  timerOption3Seconds: 'span[i18n-aria=aria_timer_3s]',
  timerOptionOff: 'span[i18n-aria=aria_timer_off]',
  toggleMicButton: '#toggle-mic',
  videoPauseResumeButton: '#pause-recordvideo',
  videoProfileSelect: '#video-profile',
  videoResolutionSettingButton: '#settings-video-resolution',
  videoSnapshotButton: '#video-snapshot',
  warningMessage: '#view-warning',
  zoomInButton: '#zoom-in',
  zoomOutButton: '#zoom-out',
} as const;
export type UiComponent = keyof typeof SELECTOR_MAP;

export const SETTING_OPTION_MAP = {
  customVideoParametersOption: {
    component: 'expertCustomVideoParametersOption',
    state: ExpertOption.CUSTOM_VIDEO_PARAMETERS,
  },
  expertModeOption: {
    component: 'expertModeOption',
    state: ExpertOption.EXPERT,
  },
  saveMetadataOption: {
    component: 'expertSaveMetadataOption',
    state: ExpertOption.SAVE_METADATA,
  },
  showMetadataOption: {
    component: 'expertShowMetadataOption',
    state: ExpertOption.SHOW_METADATA,
  },
  previewOcrOption: {
    component: 'previewOcrOption',
    state: State.ENABLE_PREVIEW_OCR,
  },
} satisfies Record<string, {component: UiComponent, state: StateUnion}>;
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
} satisfies Record<string, {component: UiComponent, view: ViewName}>;
export type SettingMenu = keyof typeof SETTING_MENU_MAP;
