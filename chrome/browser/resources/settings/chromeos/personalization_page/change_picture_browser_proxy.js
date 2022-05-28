// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An object describing a default image.
 * @typedef {{
 *   author: (string|undefined),
 *   index: number,
 *   title: (string|undefined),
 *   url: string,
 *   website: (string|undefined)
 * }}
 */
export let DefaultImage;

/** @interface */
export class ChangePictureBrowserProxy {
  /**
   * Retrieves the initial set of default images, profile image, etc. As a
   * response, the C++ sends these WebUIListener events:
   * 'default-images-changed', 'profile-image-changed', 'old-image-changed',
   * and 'selected-image-changed'
   */
  initialize() {}

  /**
   * Sets the user image to one of the default images. As a response, the C++
   * sends the 'default-images-changed' WebUIListener event.
   * @param {string} imageUrl
   */
  selectDefaultImage(imageUrl) {}

  /**
   * Sets the user image to the 'old' image. As a response, the C++ sends the
   * 'old-image-changed' WebUIListener event.
   */
  selectOldImage() {}

  /**
   * Sets the user image to the profile image. As a response, the C++ sends
   * the 'profile-image-changed' WebUIListener event.
   */
  selectProfileImage() {}

  /**
   * Provides the taken photo as a data URL to the C++ and sets the user
   * image to the 'old' image. As a response, the C++ sends the
   * 'old-image-changed' WebUIListener event.
   * @param {string} photoDataUrl
   */
  photoTaken(photoDataUrl) {}

  /**
   * Requests a file chooser to select a new user image. No response is
   * expected.
   */
  chooseFile() {}

  /** Requests the currently selected image. */
  requestSelectedImage() {}
}

/** @type {?ChangePictureBrowserProxy} */
let instance = null;

/**
 * @implements {ChangePictureBrowserProxy}
 */
export class ChangePictureBrowserProxyImpl {
  /** @return {!ChangePictureBrowserProxy} */
  static getInstance() {
    return instance || (instance = new ChangePictureBrowserProxyImpl());
  }

  /** @param {!ChangePictureBrowserProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }

  /** @override */
  initialize() {
    chrome.send('onChangePicturePageInitialized');
  }

  /** @override */
  selectDefaultImage(imageUrl) {
    chrome.send('selectImage', [imageUrl, 'default']);
  }

  /** @override */
  selectOldImage() {
    chrome.send('selectImage', ['', 'old']);
  }

  /** @override */
  selectProfileImage() {
    chrome.send('selectImage', ['', 'profile']);
  }

  /** @override */
  photoTaken(photoDataUrl) {
    chrome.send('photoTaken', [photoDataUrl]);
  }

  /** @override */
  chooseFile() {
    chrome.send('chooseFile');
  }

  /** @override */
  requestSelectedImage() {
    chrome.send('requestSelectedImage');
  }
}
