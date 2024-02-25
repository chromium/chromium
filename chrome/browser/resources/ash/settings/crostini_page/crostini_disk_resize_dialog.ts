// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'crostini-disk-resize' is a dialog for disk management e.g.
 * resizing their disk or converting it from sparse to preallocated.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrSliderElement} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {TERMINA_VM_TYPE} from '../guest_os/guest_os_browser_proxy.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, SliderTick} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_disk_resize_dialog.html.js';

/**
 * Which overall dialogue view should be shown e.g. loading, unsupported.
 */
enum DisplayState {
  LOADING = 'loading',
  UNSUPPORTED = 'unsupported',
  ERROR = 'error',
  RESIZE = 'resize',
}

/**
 * The current resizing state.
 */
enum ResizeState {
  INITIAL = 'initial',
  RESIZING = 'resizing',
  ERROR = 'error',
  DONE = 'done',
}

export interface SettingsCrostiniDiskResizeDialogElement {
  $: {
    diskResizeDialog: CrDialogElement,
  };
}

export class SettingsCrostiniDiskResizeDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-disk-resize-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      minDiskSize_: {
        type: String,
      },

      maxDiskSize_: {
        type: String,
      },

      diskSizeTicks_: {
        type: Array,
      },

      defaultDiskSizeTick_: {
        type: Number,
      },

      maxDiskSizeTick_: {
        type: Number,
      },

      isLowSpaceAvailable_: {
        type: Boolean,
        value: false,
      },

      displayState_: {
        type: String,
        value: DisplayState.LOADING,
      },

      resizeState_: {
        type: String,
        value: ResizeState.INITIAL,
      },

      /**
       * Enable the html template to use DisplayState.
       */
      DisplayState: {
        type: Object,
        value: DisplayState,
      },

      /**
       * Enable the html template to use ResizeState.
       */
      ResizeState: {
        type: Object,
        value: ResizeState,
      },
    };
  }

  private browserProxy_: CrostiniBrowserProxy;
  private minDiskSize_: string;
  private maxDiskSize_: string;
  private diskSizeTicks_: SliderTick[];
  private defaultDiskSizeTick_: number;
  private maxDiskSizeTick_: number;
  private isLowSpaceAvailable_: boolean;
  private displayState_: DisplayState;
  private resizeState_: ResizeState;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.displayState_ = DisplayState.LOADING;
    this.$.diskResizeDialog.showModal();
    this.loadDiskInfo_();
  }

  /**
   * Requests info for the current VM disk, then populates the disk info and
   * current state once the call completes.
   */
  private loadDiskInfo_(): void {
    this.browserProxy_
        .getCrostiniDiskInfo(TERMINA_VM_TYPE, /*requestFullInfo=*/ true)
        .then(
            diskInfo => {
              if (!diskInfo.succeeded) {
                this.displayState_ = DisplayState.ERROR;
              } else if (!diskInfo.canResize) {
                this.displayState_ = DisplayState.UNSUPPORTED;
              } else {
                this.displayState_ = DisplayState.RESIZE;

                this.maxDiskSizeTick_ = diskInfo.ticks.length - 1;
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
  }

  private onCancelClick_(): void {
    this.$.diskResizeDialog.close();
  }

  private onRetryClick_(): void {
    this.displayState_ = DisplayState.LOADING;
    this.loadDiskInfo_();
  }

  private onResizeClick_(): void {
    const slider = castExists(
        this.shadowRoot!.querySelector<CrSliderElement>('#diskSlider'));
    const selectedIndex = slider.value;
    const size = this.diskSizeTicks_[selectedIndex].value;
    this.resizeState_ = ResizeState.RESIZING;
    this.browserProxy_.resizeCrostiniDisk('termina', size)
        .then(
            succeeded => {
              if (succeeded) {
                this.resizeState_ = ResizeState.DONE;
                recordSettingChange(Setting.kCrostiniDiskResize);
                this.$.diskResizeDialog.close();
              } else {
                this.resizeState_ = ResizeState.ERROR;
              }
            },
            (reason) => {
              console.warn(`Unable to resize disk: ${reason}`);
              this.resizeState_ = ResizeState.ERROR;
            });
  }

  private eq_(a: string, b: string): boolean {
    return a === b;
  }

  private resizeDisabled_(displayState: string, resizeState: string): boolean {
    return displayState !== DisplayState.RESIZE ||
        resizeState === ResizeState.RESIZING;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-disk-resize-dialog':
        SettingsCrostiniDiskResizeDialogElement;
  }
}

customElements.define(
    SettingsCrostiniDiskResizeDialogElement.is,
    SettingsCrostiniDiskResizeDialogElement);
