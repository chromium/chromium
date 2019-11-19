// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-change-picture' is the settings subpage containing controls to
 * edit a ChromeOS user's picture.
 */
Polymer({
  is: 'settings-change-picture',

  behaviors: [
    settings.RouteObserverBehavior,
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
     * The active set of default user images.
     * @private {?Array<!settings.DefaultImage>}
     */
    defaultImages_: {
      type: Object,
      value: null,
    },

    /**
     * The index of the first default image to use in the selection list.
     * @private
     */
    firstDefaultImageIndex_: Number,

    /**
     * True when camera video mode is enabled.
     * @private {boolean}
     */
    cameraVideoModeEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('changePictureVideoModeEnabled');
      },
      readOnly: true,
    },
  },

  listeners: {
    'discard-image': 'onDiscardImage_',
    'image-activate': 'onImageActivate_',
    'focus-action': 'onFocusAction_',
    'photo-taken': 'onPhotoTaken_',
    'switch-mode': 'onSwitchMode_',
  },

  /** @private {?settings.ChangePictureBrowserProxy} */
  browserProxy_: null,

  /** @private {?CrPictureListElement} */
  pictureList_: null,

  /** @private {boolean} */
  oldImagePending_: false,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.ChangePictureBrowserProxyImpl.getInstance();
    this.pictureList_ =
        /** @type {CrPictureListElement} */ (this.$.pictureList);
  },

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'default-images-changed', this.receiveDefaultImages_.bind(this));
    this.addWebUIListener(
        'selected-image-changed', this.receiveSelectedImage_.bind(this));
    this.addWebUIListener(
        'old-image-changed', this.receiveOldImage_.bind(this));
    this.addWebUIListener(
        'profile-image-changed', this.receiveProfileImage_.bind(this));
    this.addWebUIListener(
        'camera-presence-changed', this.receiveCameraPresence_.bind(this));
  },


  /** @protected */
  currentRouteChanged: function(newRoute) {
    if (newRoute == settings.routes.CHANGE_PICTURE) {
      this.browserProxy_.initialize();
      this.browserProxy_.requestSelectedImage();
      this.pictureList_.setFocus();
    } else {
      // Ensure we deactivate the camera when we navigate away.
      this.selectedItem_ = null;
    }
  },

  /**
   * Handler for the 'default-images-changed' event.
   * @param {{first: number, images: !Array<!settings.DefaultImage>}} info
   * @private
   */
  receiveDefaultImages_: function(info) {
    this.defaultImages_ = info.images;
    this.firstDefaultImageIndex_ = info.first;
  },

  /**
   * Handler for the 'selected-image-changed' event. Is only called with
   * default images.
   * @param {string} imageUrl
   * @private
   */
  receiveSelectedImage_: function(imageUrl) {
    this.pictureList_.setSelectedImageUrl(imageUrl);
  },

  /**
   * Handler for the 'old-image-changed' event. The Old image is any selected
   * non-profile and non-default image. It can be from the camera, a file, or a
   * deprecated default image. When this method is called, the Old image
   * becomes the selected image.
   * @param {!{url: string, index: number}} imageInfo
   * @private
   */
  receiveOldImage_: function(imageInfo) {
    this.oldImagePending_ = false;
    this.pictureList_.setOldImageUrl(imageInfo.url, imageInfo.index);
  },

  /**
   * Handler for the 'profile-image-changed' event.
   * @param {string} imageUrl
   * @param {boolean} selected
   * @private
   */
  receiveProfileImage_: function(imageUrl, selected) {
    this.pictureList_.setProfileImageUrl(imageUrl, selected);
  },

  /**
   * Handler for the 'camera-presence-changed' event.
   * @param {boolean} cameraPresent
   * @private
   */
  receiveCameraPresence_: function(cameraPresent) {
    this.cameraPresent_ = cameraPresent;
  },

  /**
   * Selects an image element.
   * @param {!CrPicture.ImageElement} image
   * @private
   */
  selectImage_: function(image) {
    switch (image.dataset.type) {
      case CrPicture.SelectionTypes.CAMERA:
        /** CrPicturePaneElement */ (this.$.picturePane).takePhoto();
        break;
      case CrPicture.SelectionTypes.FILE:
        this.browserProxy_.chooseFile();
        break;
      case CrPicture.SelectionTypes.PROFILE:
        this.browserProxy_.selectProfileImage();
        break;
      case CrPicture.SelectionTypes.OLD:
        const imageIndex = image.dataset.imageIndex;
        if (imageIndex !== undefined && imageIndex >= 0 && image.src) {
          this.browserProxy_.selectDefaultImage(image.dataset.url);
        } else {
          this.browserProxy_.selectOldImage();
        }
        break;
      case CrPicture.SelectionTypes.DEFAULT:
        this.browserProxy_.selectDefaultImage(image.dataset.url);
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
  onImageActivate_: function(event) {
    this.selectImage_(event.detail);
  },

  /** Focus the action button in the picture pane. */
  onFocusAction_: function() {
    /** CrPicturePaneElement */ (this.$.picturePane).focusActionButton();
  },

  /**
   * @param {!CustomEvent<{photoDataUrl: string}>} event
   * @private
   */
  onPhotoTaken_: function(event) {
    this.oldImagePending_ = true;
    this.browserProxy_.photoTaken(event.detail.photoDataUrl);
    this.pictureList_.setOldImageUrl(event.detail.photoDataUrl);
    this.pictureList_.setFocus();
    announceAccessibleMessage(
        loadTimeData.getString('photoCaptureAccessibleText'));
  },

  /**
   * @param {!CustomEvent<boolean>} event
   * @private
   */
  onSwitchMode_: function(event) {
    const videomode = event.detail;
    announceAccessibleMessage(this.i18n(
        videomode ? 'videoModeAccessibleText' : 'photoModeAccessibleText'));
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
  onDiscardImage_: function() {
    // Prevent image from being discarded if old image is pending.
    if (this.oldImagePending_) {
      return;
    }
    this.pictureList_.setOldImageUrl(CrPicture.kDefaultImageUrl);
    // Revert to profile image as we don't know what last used default image is.
    this.browserProxy_.selectProfileImage();
    announceAccessibleMessage(this.i18n('photoDiscardAccessibleText'));
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageSrc_: function(selectedItem) {
    return (selectedItem && selectedItem.dataset.url) || '';
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {string}
   * @private
   */
  getImageType_: function(selectedItem) {
    return (selectedItem && selectedItem.dataset.type) ||
        CrPicture.SelectionTypes.NONE;
  },

  /**
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @param {number} firstDefaultImageIndex
   * @return {!Array<!settings.DefaultImage>}
   * @private
   */
  getDefaultImages_(defaultImages, firstDefaultImageIndex) {
    return defaultImages ? defaultImages.slice(firstDefaultImageIndex) : [];
  },

  /**
   * @param {CrPicture.ImageElement} selectedItem
   * @return {boolean} True if the author credit text is shown.
   * @private
   */
  isAuthorCreditShown_: function(selectedItem) {
    return !!selectedItem &&
        (selectedItem.dataset.type == CrPicture.SelectionTypes.DEFAULT ||
         (selectedItem.dataset.imageIndex !== undefined &&
          selectedItem.dataset.imageIndex >= 0));
  },

  /**
   * @param {!CrPicture.ImageElement} selectedItem
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @return {string} The author name for the selected default image. An empty
   *     string is returned if there is no valid author name.
   * @private
   */
  getAuthorCredit_: function(selectedItem, defaultImages) {
    const index = selectedItem ? selectedItem.dataset.imageIndex : undefined;
    if (index === undefined || index < 0 || index >= defaultImages.length) {
      return '';
    }
    const author = defaultImages[index].author;
    return author ? this.i18n('authorCreditText', author) : '';
  },

  /**
   * @param {!CrPicture.ImageElement} selectedItem
   * @param {!Array<!settings.DefaultImage>} defaultImages
   * @return {string} The author name for the selected default image. An empty
   *     string is returned if there is no valid author name.
   * @private
   */
  getAuthorWebsite_: function(selectedItem, defaultImages) {
    const index = selectedItem ? selectedItem.dataset.imageIndex : undefined;
    if (index === undefined || index < 0 || index >= defaultImages.length) {
      return '';
    }
    return defaultImages[index].website || '';
  },
});
