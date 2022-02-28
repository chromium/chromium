// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-change-picture' is the settings subpage containing controls to
 * edit a ChromeOS user's picture.
 */
import '//resources/cr_elements/chromeos/cr_picture/cr_picture_list.js';
import '//resources/cr_elements/chromeos/cr_picture/cr_picture_pane.js';
import '../../settings_shared_css.js';

import {CrPicture} from '//resources/cr_elements/chromeos/cr_picture/cr_picture_types.js';
import {convertImageSequenceToPng, isEncodedPngDataUrlAnimated} from '//resources/cr_elements/chromeos/cr_picture/png.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from '//resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../metrics_recorder.m.js';
import {routes} from '../os_route.m.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

import {ChangePictureBrowserProxy, ChangePictureBrowserProxyImpl, DefaultImage} from './change_picture_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-change-picture',

  behaviors: [
    DeepLinkingBehavior,
    RouteObserverBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * True if the user has a plugged-in webcam.
     * @private {boolean}
     */
    cameraPresent_: {
      type: Boolean,
      value: false,
    },

    /**
     * The currently selected item. This property is bound to the iron-selector
     * and never directly assigned. This may be undefined momentarily as
     * the selection changes due to iron-selector implementation details.
     * @private {?CrPicture.ImageElement}
     */
    selectedItem_: {
      type: Object,
      value: null,
    },

    /**
     * The current set of the default user images.
     * @private {?Array<!DefaultImage>}
     */
    currentDefaultImages_: {
      type: Object,
      value: null,
    },

    /**
     * True when camera video mode is enabled.
     * @private {boolean}
     */
    cameraVideoModeEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('changePictureVideoModeEnabled');
      },
      readOnly: true,
    },

    /**
     * Author info of the default image.
     * @private {string}
     */
    authorInfo_: String,

    /**
     * Website info of the default image.
     * @private {string}
     */
    websiteInfo_: String,

    /** @private */
    oldImageLabel_: String,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () =>
          new Set([chromeos.settings.mojom.Setting.kChangeDeviceAccountImage]),
    },
  },

  listeners: {
    'discard-image': 'onDiscardImage_',
    'image-activate': 'onImageActivate_',
    'focus-action': 'onFocusAction_',
    'photo-taken': 'onPhotoTaken_',
    'switch-mode': 'onSwitchMode_',
  },

  /** @private {?ChangePictureBrowserProxy} */
  browserProxy_: null,

  /** @private {?CrPictureListElement} */
  pictureList_: null,

  /** @private {boolean} */
  oldImagePending_: false,

  /** @override */
  ready() {
    this.browserProxy_ = ChangePictureBrowserProxyImpl.getInstance();
    this.pictureList_ =
        /** @type {CrPictureListElement} */ (this.$.pictureList);
  },

  /** @override */
  attached() {
    this.addWebUIListener(
        'default-images-changed', this.receiveDefaultImages_.bind(this));
    this.addWebUIListener(
        'selected-image-changed', this.receiveSelectedImage_.bind(this));
    this.addWebUIListener(
        'old-image-changed', this.receiveOldImage_.bind(this));
    this.addWebUIListener(
        'preview-deprecated-image',
        this.receivePreviewDeprecatedImage_.bind(this));
    this.addWebUIListener(
        'profile-image-changed', this.receiveProfileImage_.bind(this));
    this.addWebUIListener(
        'camera-presence-changed', this.receiveCameraPresence_.bind(this));

    // Initialize the announcer once.
    IronA11yAnnouncer.requestAvailability();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    assert(
        settingId ===
        chromeos.settings.mojom.Setting.kChangeDeviceAccountImage);

    this.pictureList_.setFocus();
    return false;
  },


  /** @protected */
  currentRouteChanged(newRoute) {
    if (newRoute === routes.CHANGE_PICTURE) {
      this.browserProxy_.initialize();
      this.browserProxy_.requestSelectedImage();
      this.pictureList_.setFocus();
      this.attemptDeepLink();
    } else {
      // Ensure we deactivate the camera when we navigate away.
      this.selectedItem_ = null;
    }
  },

  /**
   * Handler for the 'default-images-changed' event.
   * @param {{current_default_images: !Array<!DefaultImage>}} info
   * @private
   */
  receiveDefaultImages_(info) {
    this.currentDefaultImages_ = info.current_default_images;
  },

  /**
   * Handler for the 'selected-image-changed' event. Is only called with
   * default images.
   * @param {string} imageUrl
   * @private
   */
  receiveSelectedImage_(imageUrl) {
    this.pictureList_.setSelectedImageUrl(imageUrl);
  },

  /**
   * Handler for the 'old-image-changed' event. The Old image is any selected
   * non-profile and non-default image. It can be from the camera or a file.
   * When this method is called, the Old image becomes the selected image.
   * @param {string} imageUrl
   * @private
   */
  receiveOldImage_(imageUrl) {
    this.oldImageLabel_ = this.i18n(
        isEncodedPngDataUrlAnimated(imageUrl) ? 'oldVideo' : 'oldPhoto');
    this.oldImagePending_ = false;
    this.pictureList_.setOldImageUrl(imageUrl);
  },

  /**
   * Handler for the 'preview-deprecated-image' event.
   * When this method is called, preview the deprecated default image in
   * picturePane while do not show in the pictureList.
   * Also set the source info for the deprecated image.
   * @param {!{url: string, author: string, website: string}} imageInfo
   * @private
   */
  receivePreviewDeprecatedImage_(imageInfo) {
    this.$.picturePane.previewDeprecatedImage(imageInfo.url);
    this.authorInfo_ =
        imageInfo.author ? this.i18n('authorCreditText', imageInfo.author) : '';
    this.websiteInfo_ = imageInfo.website;
    this.selectedItem_ = null;
  },

  /**
   * Whether the source info should be shown.
   * @param {CrPicture.ImageElement} selectedItem
   * @param {string} authorInfo
   * @param {string} websiteInfo
   * @private
   */
  shouldShowSourceInfo_(selectedItem, authorInfo, websiteInfo) {
    return !selectedItem && (authorInfo || websiteInfo);
  },

  /**
   * Handler for the 'profile-image-changed' event.
   * @param {string} imageUrl
   * @param {boolean} selected
   * @private
   */
  receiveProfileImage_(imageUrl, selected) {
    this.pictureList_.setProfileImageUrl(imageUrl, selected);
  },

  /**
   * Handler for the 'camera-presence-changed' event.
   * @param {boolean} cameraPresent
   * @private
   */
  receiveCameraPresence_(cameraPresent) {
    this.cameraPresent_ = cameraPresent;
  },

  /**
   * Selects an image element.
   * @param {!CrPicture.ImageElement} image
   * @private
   */
  selectImage_(image) {
    switch (image.dataset.type) {
      case CrPicture.SelectionTypes.CAMERA:
        /** CrPicturePaneElement */ (this.$.picturePane).takePhoto();
        break;
      case CrPicture.SelectionTypes.FILE:
        this.browserProxy_.chooseFile();
        recordSettingChange();
        break;
      case CrPicture.SelectionTypes.PROFILE:
        this.browserProxy_.selectProfileImage();
        recordSettingChange();
        break;
      case CrPicture.SelectionTypes.OLD:
        this.browserProxy_.selectOldImage();
        recordSettingChange();
        break;
      case CrPicture.SelectionTypes.DEFAULT:
        this.browserProxy_.selectDefaultImage(image.dataset.url);
        recordSettingChange();
        break;
      default:
        assertNotReached('Selected unknown image type');
    }
  },

  /**
   * Handler for when an image is activated.
   * @param {!CustomEvent<!CrPicture.ImageElement>} event
   * @private
   */
  onImageActivate_(event) {
    this.selectImage_(event.detail);
  },

  /** Focus the action button in the picture pane. */
  onFocusAction_() {
    /** CrPicturePaneElement */ (this.$.picturePane).focusActionButton();
  },

  /**
   * @param {!CustomEvent<{photoDataUrl: string}>} event
   * @private
   */
  onPhotoTaken_(event) {
    this.oldImagePending_ = true;
    this.browserProxy_.photoTaken(event.detail.photoDataUrl);
    this.pictureList_.setOldImageUrl(event.detail.photoDataUrl);
    this.pictureList_.setFocus();
    this.fire(
        'iron-announce',
        {text: loadTimeData.getString('photoCaptureAccessibleText')});
  },

  /**
   * @param {!CustomEvent<boolean>} event
   * @private
   */
  onSwitchMode_(event) {
    const videomode = event.detail;
    this.fire('iron-announce', {
      text: this.i18n(
          videomode ? 'videoModeAccessibleText' : 'photoModeAccessibleText')
    });
  },

  /**
   * Callback the iron-a11y-keys "keys-pressed" event bubbles up from the
   * cr-camera-pane.
   * @param {!CustomEvent<!{key: string, keyboardEvent: Object}>} event
   * @private
   */
  onCameraPaneKeysPressed_(event) {
    this.$.pictureList.focus();
    this.$.pictureList.onKeysPressed(event);
  },

  /** @private */
  onDiscardImage_() {
    // Prevent image from being discarded if old image is pending.
    if (this.oldImagePending_) {
      return;
    }
    this.pictureList_.setOldImageUrl(CrPicture.kDefaultImageUrl);
    // Revert to profile image as we don't know what last used default image is.
    this.browserProxy_.selectProfileImage();
    this.fire('iron-announce', {text: this.i18n('photoDiscardAccessibleText')});
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageSrc_(selectedItem) {
    return (selectedItem && selectedItem.dataset.url) || '';
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageType_(selectedItem) {
    return (selectedItem && selectedItem.dataset.type) ||
        CrPicture.SelectionTypes.NONE;
  },
});
