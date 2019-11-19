// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Creates the Gallery Base view controller.
 * @param {string} selector Selector text of the view's root element.
 * @param {cca.models.Gallery} model Model object.
 * @extends {cca.views.View}
 * @implements {cca.models.Gallery.Observer}
 * @constructor
 */
cca.views.GalleryBase = function(selector, model) {
  cca.views.View.call(this, selector, true);

  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * Contains pictures' views.
   * @type {Array<cca.views.GalleryBase.DOMPicture>}
   * @protected
   */
  this.pictures = [];

  /**
   * Contains selected pictures' indexes sorted in the selection order.
   * @type {Array<number>}
   * @protected
   */
  this.selectedIndexes = [];
};

/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element.
 * @param {cca.models.Gallery.Picture} picture Picture data.
 * @param {HTMLImageElement} element DOM element holding the picture.
 * @constructor
 */
cca.views.GalleryBase.DOMPicture = function(picture, element) {
  /**
   * @type {cca.models.Gallery.Picture}
   * @private
   */
  this.picture_ = picture;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  // End of properties. Seal the object.
  Object.seal(this);
};

cca.views.GalleryBase.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  },
};

cca.views.GalleryBase.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * Exports the selected pictures. If nothing selected, then nothing happens.
 * @protected
 */
cca.views.GalleryBase.prototype.exportSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length) {
    return;
  }
  chrome.fileSystem.chooseEntry({type: 'openDirectory'}, (dirEntry) => {
    if (!dirEntry) {
      return;
    }
    this.selectedPictures().forEach((domPicture) => {
      var picture = domPicture.picture;
      // TODO(yuli): Use FileSystem.getFile_ to handle name conflicts.
      dirEntry.getFile(
          cca.models.FileSystem.regulatePictureName(picture.pictureEntry),
          {create: true, exclusive: false}, (entry) => {
            this.model_.exportPicture(picture, entry).catch((error) => {
              console.error(error);
              cca.toast.show(chrome.i18n.getMessage(
                  'error_msg_gallery_export_failed', entry.name));
            });
          });
    });
  });
};

/**
 * Deletes the currently selected pictures. If nothing selected, then nothing
 * happens.
 * @protected
 */
cca.views.GalleryBase.prototype.deleteSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length) {
    return;
  }
  var multi = selectedIndexes.length > 1;
  var param = multi ? selectedIndexes.length.toString() :
      this.lastSelectedPicture().picture.pictureEntry.name;
  var message = chrome.i18n.getMessage(
      multi ? 'delete_multi_confirmation_msg' : 'delete_confirmation_msg',
      param);
  cca.nav.open('message-dialog', {message, cancellable: true})
      .then((confirmed) => {
        if (!confirmed) {
          return;
        }
        var selectedPictures = this.selectedPictures();
        for (var i = selectedPictures.length - 1; i >= 0; i--) {
          this.model_.deletePicture(selectedPictures[i].picture)
              .catch((error) => {
                console.error(error);
                // TODO(yuli): Show a toast message here.
              });
        }
      });
};

/**
 * Returns the last selected picture index from the current selections.
 * @return {?number}
 * @protected
 */
cca.views.GalleryBase.prototype.lastSelectedIndex = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length) {
    return null;
  }
  return selectedIndexes[selectedIndexes.length - 1];
};

/**
 * Returns the last selected picture from the current selections.
 * @return {cca.views.GalleryBase.DOMPicture}
 * @protected
 */
cca.views.GalleryBase.prototype.lastSelectedPicture = function() {
  var leadIndex = this.lastSelectedIndex();
  return (leadIndex !== null) ? this.pictures[leadIndex] : null;
};

/**
 * Returns the currently selected picture views sorted in the added order.
 * @return {Array<cca.views.GalleryBase.DOMPicture>}
 * @protected
 */
cca.views.GalleryBase.prototype.selectedPictures = function() {
  var indexes = this.selectedIndexes.slice();
  indexes.sort((a, b) => a - b);
  return indexes.map((i) => this.pictures[i]);
};

/**
 * Returns the picture's index in the picture views.
 * @param {cca.models.Gallery.Picture} picture Picture to be indexed.
 * @return {?number}
 * @protected
 */
cca.views.GalleryBase.prototype.pictureIndex = function(picture) {
  for (var index = 0; index < this.pictures.length; index++) {
    if (this.pictures[index].picture == picture) {
      return index;
    }
  }
  return null;
};

/**
 * Sets the selected index.
 * @param {number} index Index of the picture to be selected.
 * @protected
 */
cca.views.GalleryBase.prototype.setSelectedIndex = function(index) {
  var updateSelection = (element, select) => {
    cca.nav.setTabIndex(this, element, select ? 0 : -1);
    element.classList.toggle('selected', select);
    element.setAttribute('aria-selected', select ? 'true' : 'false');
  };
  // Unselect selected pictures and select a new picture by the given index.
  var selectedIndexes = this.selectedIndexes;
  selectedIndexes.forEach((selectedIndex) => {
    updateSelection(this.pictures[selectedIndex].element, false);
  });
  selectedIndexes.splice(0, selectedIndexes.length);

  if (index !== null) {
    updateSelection(this.pictures[index].element, true);
    selectedIndexes.push(index);
  }
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.onPictureDeleted = function(picture) {
  var index = this.pictureIndex(picture);
  if (index == null) {
    return;
  }

  // Hack to restore focus after removing an element. Note, that we restore
  // focus only if there was something focused before. However, if the focus
  // was on the selected element, then after removing it from DOM, there will
  // be nothing focused, while we still want to restore the focus.
  var element = this.pictures[index].element;
  element.parentNode.removeChild(element);
  this.pictures.splice(index, 1);

  // Update the selection if the deleted picture is selected.
  var removal = this.selectedIndexes.indexOf(index);
  if (removal != -1) {
    this.selectedIndexes.splice(removal, 1);
    for (var i = 0; i < this.selectedIndexes.length; i++) {
      if (this.selectedIndexes[i] > index) {
        this.selectedIndexes[i]--;
      }
    }
  }
  if (!this.selectedIndexes.length) {
    if (this.pictures.length > 0) {
      this.setSelectedIndex(Math.max(0, index - 1));
    } else {
      this.setSelectedIndex(null);
      // Assume browser-view's picture-deletion only occurs when browser-view is
      // active and don't need to handle inactive empty browser-view for now.
      this.leave();
    }
  }
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.handlingKey = function(key) {
  switch (key) {
    case 'Delete':
    case 'Meta-Backspace':
      this.deleteSelection();
      return true;
    case 'Ctrl-S': // Ctrl+S for saving.
      this.exportSelection();
      return true;
    case 'Ctrl-P': // Ctrl+P for printing.
      window.print();
      return true;
  }
  return false;
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.onPictureAdded = function(picture) {
  this.addPictureToDOM(picture);
};

/**
 * Adds the picture to DOM.
 * @abstract
 * @param {cca.models.Gallery.Picture} picture Model's picture to be added.
 * @protected
 */
cca.views.GalleryBase.prototype.addPictureToDOM = function(picture) {
  throw new Error('Not implemented.');
};

/**
 * Synchronizes focus with the selection if the view is active.
 * @protected
 */
cca.views.GalleryBase.prototype.synchronizeFocus = function() {
  // Synchronize focus on the last selected picture.
  var selectedPicture = this.lastSelectedPicture();
  var element = selectedPicture && selectedPicture.element;
  if (element && element.tabIndex >= 0) {
    element.focus();
  }
};
