// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-disk-resize' is a dialog for disk management e.g.
 * resizing their disk or converting it from sparse to preallocated.
 */
(function() {

/**
 * Which overall dialogue view should be shown e.g. loading, unsupported.
 * @enum {string}
 */
const DisplayState = {
  LOADING: 'loading',
  UNSUPPORTED: 'unsupported',
  ERROR: 'error',
  RESIZE: 'resize',
};

/**
 * The current resizing state.
 * @enum {string}
 */
const ResizeState = {
  INITIAL: 'initial',
  RESIZING: 'resizing',
  ERROR: 'error',
  DONE: 'done',
};

Polymer({
  is: 'settings-crostini-disk-resize-dialog',

  properties: {
    /** @private */
    minDiskSize_: {
      type: String,
    },

    /** @private */
    maxDiskSize_: {
      type: String,
    },

    /** @private {Array<!cr_slider.SliderTick>} */
    diskSizeTicks_: {
      type: Array,
    },

    /** @private */
    defaultDiskSizeTick_: {
      type: Number,
    },

    /** @private */
    maxDiskSizeTick_: {
      type: Number,
    },

    /** @private */
    isLowSpaceAvailable_: {
      type: Boolean,
      value: false,
    },

    /** @private {!DisplayState} */
    displayState_: {
      type: String,
      value: DisplayState.LOADING,
    },

    /** @private {!ResizeState} */
    resizeState_: {
      type: String,
      value: ResizeState.INITIAL,
    },

    /**
     * Enable the html template to use DisplayState.
     * @private
     */
    DisplayState: {
      type: Object,
      value: DisplayState,
    },

    /**
     * Enable the html template to use ResizeState.
     * @private
     */
    ResizeState: {
      type: Object,
      value: ResizeState,
    },
  },

  /** @override */
  attached() {
    this.displayState_ = DisplayState.LOADING;
    this.$.diskResizeDialog.showModal();
    this.loadDiskInfo_();
  },

  /**
   * @private
   * Requests info for the current VM disk, then populates the disk info and
   * current state once the call completes.
   */
  loadDiskInfo_() {
    // TODO(davidmunro): No magic 'termina' string.
    const vmName = 'termina';
    settings.CrostiniBrowserProxyImpl.getInstance()
        .getCrostiniDiskInfo(vmName, /*requestFullInfo=*/ true)
        .then(
            diskInfo => {
              if (!diskInfo.succeeded) {
                this.displayState_ = DisplayState.ERROR;
              } else if (!diskInfo.canResize) {
                this.displayState_ = DisplayState.UNSUPPORTED;
              } else {
                this.displayState_ = DisplayState.RESIZE;

                this.maxDiskSizeTick = diskInfo.ticks.length - 1;
                this.defaultDiskSizeTick_ = diskInfo.defaultIndex;
                this.diskSizeTicks_ = diskInfo.ticks;
                this.minDiskSize_ = diskInfo.ticks[0].label;
                this.maxDiskSize_ =
                    diskInfo.ticks[diskInfo.ticks.length - 1].label;
                this.isLowSpaceAvailable_ = diskInfo.isLowSpaceAvailable;
              }
            },
            reason => {
              console.log(`Unable to get info: ${reason}`);
              this.displayState_ = DisplayState.ERROR;
            });
  },

  /** @private */
  onCancelClick_() {
    this.$.diskResizeDialog.close();
  },

  /** @private */
  onRetryClick_() {
    this.displayState_ = DisplayState.LOADING;
    this.loadDiskInfo_();
  },

  /** @private */
  onResizeClick_() {
    const selectedIndex = this.$$('#diskSlider').value;
    const size = this.diskSizeTicks_[selectedIndex].value;
    this.resizeState_ = ResizeState.RESIZING;
    settings.CrostiniBrowserProxyImpl.getInstance()
        .resizeCrostiniDisk('termina', size)
        .then(
            succeeded => {
              if (succeeded) {
                this.resizeState_ = ResizeState.DONE;
                this.$.diskResizeDialog.close();
              } else {
                this.resizeState_ = ResizeState.ERROR;
              }
            },
            (reason) => {
              console.log(`Unable to resize disk: ${reason}`);
              this.resizeState_ = ResizeState.ERROR;
            });
  },

  /**
   * @private
   */
  eq_(a, b) {
    return a === b;
  },

  /** @private */
  resizeDisabled_(displayState, resizeState) {
    return displayState !== DisplayState.RESIZE ||
        resizeState === ResizeState.RESIZING;
  },
});
})();
