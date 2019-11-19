// Copyright 2018 The Chromium Authors. All rights reserved.
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
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for the record-time of Camera view.
 * @constructor
 */
cca.views.camera.RecordTime = function() {
  /**
   * @type {!HTMLElement}
   * @private
   */
  this.recordTime_ =
      /** @type {!HTMLElement} */ (document.querySelector('#record-time'));

  /**
   * Timeout to count every tick of elapsed recording time.
   * @type {?number}
   * @private
   */
  this.tickTimeout_ = null;

  /**
   * Tick count of elapsed recording time.
   * @type {number}
   * @private
   */
  this.ticks_ = 0;

  // End of properties, seal the object.
  Object.seal(this);
};

/**
 * Updates UI by the elapsed recording time.
 * @param {number} time Time in seconds.
 * @private
 */
cca.views.camera.RecordTime.prototype.update_ = function(time) {
  // Format time into HH:MM:SS or MM:SS.
  var pad = (n) => {
    return (n < 10 ? '0' : '') + n;
  };
  var hh = '';
  if (time >= 3600) {
    hh = pad(Math.floor(time / 3600)) + ':';
  }
  var mm = pad(Math.floor(time / 60) % 60) + ':';
  document.querySelector('#record-time-msg').textContent =
      hh + mm + pad(time % 60);
};

/**
 * Starts to count and show the elapsed recording time.
 */
cca.views.camera.RecordTime.prototype.start = function() {
  this.update_(0);
  this.recordTime_.hidden = false;

  this.ticks_ = 0;
  this.tickTimeout_ = setInterval(() => {
    this.ticks_++;
    this.update_(this.ticks_);
  }, 1000);
};

/**
 * Stops counting and showing the elapsed recording time.
 * @return {number} Recorded time in 1 minute buckets.
 */
cca.views.camera.RecordTime.prototype.stop = function() {
  cca.toast.speak('status_msg_recording_stopped');
  if (this.tickTimeout_) {
    clearInterval(this.tickTimeout_);
    this.tickTimeout_ = null;
  }
  var mins = Math.ceil(this.ticks_ / 60);
  this.ticks_ = 0;
  this.recordTime_.hidden = true;
  this.update_(0);
  return mins;
};
