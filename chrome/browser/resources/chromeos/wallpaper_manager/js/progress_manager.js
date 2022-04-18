// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Monitor the downloading progress of a XMLHttpRequest |xhr_| and shows the
 * progress on |progressBar_|.
 * @constructor
 */
function ProgressManager() {
  this.xhr_ = null;
  this.progressBar_ = document.querySelector('.progress-bar');
  this.selectedGridItem_ = null;
}

/**
 * Sets the XMLHttpRequest |xhr| to monitor, and the wallpaper thumbnail grid
 * item |selectedGridItem| to show a progress bar for. Cancels previous xhr and
 * hides/removes previous progress bar if any.
 * Note: this must be called before xhr.send() function. Otherwise, we wont get
 * loadstart event.
 * @param {XMLHttpRequest} xhr The XMLHttpRequest.
 * @param {WallpaperThumbnailsGridItem} selectedGridItem The wallpaper thumbnail
 *     grid item. It extends from cr.ui.ListItem.
 */
ProgressManager.prototype.reset = function(xhr, selectedGridItem) {
  if (this.xhr_) {
    this.removeEventListeners_();
  }
  this.hideProgressBar(this.selectedGridItem_);
  this.xhr_ = xhr;
  this.selectedGridItem_ = selectedGridItem;
  this.xhrListeners_ = {
    'loadstart': this.onDownloadStart_.bind(this),
    'progress': this.onDownloadProgress_.bind(this),
    'abort': this.onDownloadErrorOrAbort_.bind(this),
    'error': this.onDownloadErrorOrAbort_.bind(this),
    'load': this.onDownloadComplete_.bind(this)
  };
  for (var eventType in this.xhrListeners_) {
    this.xhr_.addEventListener(eventType, this.xhrListeners_[eventType]);
  }
};

/**
 * Removes all event listeners progress manager currently registered.
 * @private
 */
ProgressManager.prototype.removeEventListeners_ = function() {
  for (var eventType in this.xhrListeners_) {
    this.xhr_.removeEventListener(eventType, this.xhrListeners_[eventType]);
  }
};

/**
 * Removes the progress bar in |selectedGridItem| if any. May be called
 * asynchronously.
 * @param {WallpaperThumbnailsGridItem} selectedGridItem The wallpaper thumbnail
       grid item. It extends from cr.ui.ListItem.
 */
ProgressManager.prototype.hideProgressBar = function(selectedGridItem) {
  if (selectedGridItem && selectedGridItem.querySelector('.progress-bar')) {
    this.progressBar_.hidden = true;
    selectedGridItem.removeChild(this.progressBar_);
  }
};

/**
 * Calculates and updates the width of progress track.
 * @private
 * @param {float} percentComplete The percent of loaded content.
 */
ProgressManager.prototype.setProgress_ = function(percentComplete) {
  this.progressBar_.querySelector('.progress-track').style.width =
      (percentComplete * 100) + '%';
};

/**
 * Shows a 0% progress bar to indicate downloading starts.
 * @private
 * @param {Event} e A loadstart ProgressEvent from XMLHttpRequest.
 */
ProgressManager.prototype.onDownloadStart_ = function(e) {
  this.setProgress_(0);
  this.selectedGridItem_.appendChild(this.progressBar_);
  this.progressBar_.hidden = false;
};

/**
 * Hides progress bar when progression is terminated.
 * @private
 * @param {Event} e An error/abort ProgressEvent from XMLHttpRequest.
 */
ProgressManager.prototype.onDownloadErrorOrAbort_ = function(e) {
  this.removeEventListeners_();
  this.xhr_ = null;
  this.hideProgressBar(this.selectedGridItem_);
};

/**
 * Download completed successfully. Shows a 100% progress bar and clears |xhr_|.
 * @private
 * @param {Event} e A load ProgressEvent from XMLHttpRequest.
 */
ProgressManager.prototype.onDownloadComplete_ = function(e) {
  this.setProgress_(1);
  this.removeEventListeners_();
  this.xhr_ = null;
};

/**
 * Calculates downloading percentage and shows downloading progress.
 * @private
 * @param {Event} e A progress ProgressEvent from XMLHttpRequest.
 */
ProgressManager.prototype.onDownloadProgress_ = function(e) {
  if (e.lengthComputable) {
    this.setProgress_(e.loaded / e.total);
  }
};
