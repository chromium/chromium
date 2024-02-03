// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';

import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './camera_roll_manager_form.html.js';
import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {CameraRollManager, DownloadResult, FileType} from './types.js';

/**
 * Maps a FileType to its title label in the dropdown.
 * @type {!Map<FileType, String>}
 */
const fileTypeToStringMap = new Map([
  [FileType.IMAGE, 'Image'],
  [FileType.VIDEO, 'Video'],
]);

/**
 * Maps a DownloadResult to its title label in the dropdown.
 * @type {!Map<DownloadResult, String>}
 */
const downloadResultToStringMap = new Map([
  [DownloadResult.SUCCESS, 'Download Success'],
  [DownloadResult.ERROR_GENERIC, 'Generic Error'],
  [DownloadResult.ERROR_STORAGE, 'Storage Error'],
  [DownloadResult.ERROR_NETWORK, 'Network Error'],
]);

Polymer({
  is: 'camera-roll-manager-form',

  _template: getTemplate(),

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    isCameraRollEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isFileAccessGranted_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isOnboardingDismissed_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isLoadingViewShown_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    numberOfThumbnails_: {
      type: Number,
      value: 4,
    },

    /** @private{FileType} */
    fileType_: {
      type: Number,
      value: FileType.IMAGE,
    },

    /** @private */
    fileTypeList_: {
      type: Array,
      value: () => {
        return [
          FileType.IMAGE,
          FileType.VIDEO,
        ];
      },
      readonly: true,
    },

    /** @private{DownloadResult} */
    downloadResult_: {
      type: Number,
      value: DownloadResult.SUCCESS,
    },

    /** @private */
    downloadResultList_: {
      type: Array,
      value: () => {
        return [
          DownloadResult.SUCCESS,
          DownloadResult.ERROR_GENERIC,
          DownloadResult.ERROR_STORAGE,
          DownloadResult.ERROR_NETWORK,
        ];
      },
      readonly: true,
    },
  },

  /** @private {?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'camera-roll-ui-view-state-updated',
        this.onCameraRollViewUiStateUpdated_.bind(this));
  },

  /**
   * @param {!CameraRollManager} cameraRollManager
   * @private
   */
  onCameraRollViewUiStateUpdated_(cameraRollManager) {
    this.isCameraRollEnabled_ = cameraRollManager.isCameraRollEnabled;
    this.isOnboardingDismissed_ = cameraRollManager.isOnboardingDismissed;
    this.isLoadingViewShown_ = cameraRollManager.isLoadingViewShown;
  },

  /** @private */
  onNumberOfThumbnailsChanged_() {
    const inputValue = this.$$('#numberOfThumbnailsInput').value;
    if (inputValue > 16) {
      this.numberOfThumbnails_ = 16;
      return;
    }

    if (inputValue < 0) {
      this.numberOfThumbnails_ = 0;
      return;
    }

    this.numberOfThumbnails_ = Number(inputValue);
  },

  /** @private */
  onFileTypeSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#fileTypeList'));
    this.fileType_ = this.fileTypeList_[select.selectedIndex];
  },

  /**
   * @param {FileType} fileType
   * @return {String}
   * @private
   */
  getFileTypeName_(fileType) {
    return fileTypeToStringMap.get(fileType);
  },

  /** @private */
  onDownloadResultSelected_() {
    const select = /** @type {!HTMLSelectElement} */
        (this.$$('#downloadResultList'));
    this.downloadResult_ = this.downloadResultList_[select.selectedIndex];
  },

  /**
   * @param {DownloadResult} downloadResult
   * @return {String}
   * @private
   */
  getDownloadResultName_(downloadResult) {
    return downloadResultToStringMap.get(downloadResult);
  },

  /** @private */
  setFakeCameraRollManager_() {
    const cameraRollManager = {
      isCameraRollEnabled: this.isCameraRollEnabled_,
      isOnboardingDismissed: this.isOnboardingDismissed_,
      isFileAccessGranted: this.isFileAccessGranted_,
      isLoadingViewShown: this.isLoadingViewShown_,
      numberOfThumbnails: this.numberOfThumbnails_,
      fileType: this.fileType_,
      downloadResult: this.downloadResult_,
    };
    this.browserProxy_.setFakeCameraRoll(cameraRollManager);
  },
});
