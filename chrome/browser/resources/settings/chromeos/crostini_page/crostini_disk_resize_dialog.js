// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-disk-resize' is a dialog for disk management e.g.
 * resizing their disk or converting it from sparse to preallocated.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../settings_shared_css.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';


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
  _template: html`{__html_template__}`,
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

    /** @private {Array<!SliderTick>} */
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
    CrostiniBrowserProxyImpl.getInstance()
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
              console.warn(`Unable to get info: ${reason}`);
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
    CrostiniBrowserProxyImpl.getInstance()
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
              console.warn(`Unable to resize disk: ${reason}`);
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
