// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates a controller for the gallery-button.
 * @param {cca.models.Gallery} model Model object.
 * @implements {cca.models.Gallery.Observer}
 * @constructor
 */
cca.GalleryButton = function(model) {
  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * @type {cca.models.Gallery.Picture}
   * @private
   */
  this.lastPicture_ = null;

  /**
   * @type {HTMLButtonElement}
   * @private
   */
  this.button_ = document.querySelector('#gallery-enter');

  // End of properties, seal the object.
  Object.seal(this);

  this.button_.addEventListener('click', (event) => {
    // Check if the last picture still exists before opening it in the gallery.
    // TODO(yuli): Remove this workaround for unable watching changed-files.
    this.model_.checkLastPicture().then((picture) => {
      if (picture) {
        this.openGallery_(picture);
      }
    });
  });
};

/**
 * Updates the button for the model changes.
 * @private
 */
cca.GalleryButton.prototype.updateButton_ = function() {
  this.model_.lastPicture().then((picture) => {
    if (picture != this.lastPicture_) {
      this.lastPicture_ = picture;
      return true;
    }
    return false;
  }).then((changed) => {
    if (!changed) {
      return;
    }
    this.button_.hidden = !this.lastPicture_;
    var url = this.lastPicture_ && this.lastPicture_.thumbnailURL;
    this.button_.style.backgroundImage = url ? ('url("' + url + '")') : 'none';
  });
};

/**
 * Opens the gallery to browse the picture.
 * @param {cca.models.Gallery.Picture} picture Picture to be browsed.
 * @private
 */
cca.GalleryButton.prototype.openGallery_ = function(picture) {
  if (cca.App.useGalleryApp()) {
    const id = 'nlkncpkkdoccmpiclbokaimcnedabhhm|app|open';
    const entry = picture.pictureEntry;
    chrome.fileManagerPrivate.executeTask(id, [entry], (result) => {
      if (chrome.runtime.lastError) {
        console.warn(
            'Unable to open picture: ' + chrome.runtime.lastError.message);
        return;
      }
      if (result != 'opened' && result != 'message_sent') {
        console.warn('Unable to open picture: ' + result);
      }
    });
  } else {
    cca.nav.open('browser', picture);
  }
};

/**
 * @override
 */
cca.GalleryButton.prototype.onPictureDeleted = function(picture) {
  if (this.lastPicture_ == picture) {
    this.updateButton_();
  }
};

/**
 * @override
 */
cca.GalleryButton.prototype.onPictureAdded = function(picture) {
  if (!this.lastPicture_ || this.lastPicture_.timestamp <= picture.timestamp) {
    this.updateButton_();
  }
};
