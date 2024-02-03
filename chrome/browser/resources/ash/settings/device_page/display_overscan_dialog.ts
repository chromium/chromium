// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-display-overscan-dialog' is the dialog for display overscan
 * adjustments.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_icons.html.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';

import {getDisplayApi} from './device_page_browser_proxy.js';
import {getTemplate} from './display_overscan_dialog.html.js';

import Insets = chrome.system.display.Insets;

export interface SettingsDisplayOverscanDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class SettingsDisplayOverscanDialogElement extends PolymerElement {
  static get is() {
    return 'settings-display-overscan-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Id of the display for which overscan is being applied (or empty). */
      displayId: {
        type: String,
        notify: true,
        observer: 'displayIdChanged_',
      },

      /**
         Set to true once changes are saved to avoid a reset/cancel on close.
       */
      committed_: Boolean,

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },
    };
  }

  displayId: string;
  private committed_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private keyHandler_: (event: KeyboardEvent) => void;

  constructor() {
    super();

    /**
     * Keyboard event handler for overscan adjustments.
     */
    this.keyHandler_ = this.handleKeyEvent_.bind(this);
  }

  open(): void {
    // We need to attach the event listener to |window|, not |this| so that
    // changing focus does not prevent key events from occurring.
    window.addEventListener('keydown', this.keyHandler_);
    this.committed_ = false;
    this.$.dialog.showModal();
    // Don't focus 'reset' by default. 'Tab' will focus 'OK'.
    this.shadowRoot!.getElementById('reset')!.blur();
  }

  close(): void {
    window.removeEventListener('keydown', this.keyHandler_);

    this.displayId = '';  // Will trigger displayIdChanged_.

    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }

  private displayIdChanged_(newValue: string, oldValue: string): void {
    if (oldValue && !this.committed_) {
      getDisplayApi().overscanCalibrationReset(oldValue);
      getDisplayApi().overscanCalibrationComplete(oldValue);
    }
    if (!newValue) {
      return;
    }
    this.committed_ = false;
    getDisplayApi().overscanCalibrationStart(newValue);
  }

  private onResetClick_(): void {
    getDisplayApi().overscanCalibrationReset(this.displayId);
  }

  private onSaveClick_(): void {
    getDisplayApi().overscanCalibrationComplete(this.displayId);
    this.committed_ = true;
    this.close();
  }

  private handleKeyEvent_(event: KeyboardEvent): void {
    if (event.altKey || event.ctrlKey || event.metaKey) {
      return;
    }
    switch (event.keyCode) {
      case 37:  // left arrow
        if (event.shiftKey) {
          this.move_(-1, 0);
        } else {
          this.resize_(1, 0);
        }
        break;
      case 38:  // up arrow
        if (event.shiftKey) {
          this.move_(0, -1);
        } else {
          this.resize_(0, -1);
        }
        break;
      case 39:  // right arrow
        if (event.shiftKey) {
          this.move_(1, 0);
        } else {
          this.resize_(-1, 0);
        }
        break;
      case 40:  // down arrow
        if (event.shiftKey) {
          this.move_(0, 1);
        } else {
          this.resize_(0, 1);
        }
        break;
      default:
        // Allow unhandled key events to propagate.
        return;
    }
    event.preventDefault();
  }

  private move_(x: number, y: number): void {
    const delta: Insets = {
      left: x,
      top: y,
      right: x ? -x : 0,  // negating 0 will produce a double.
      bottom: y ? -y : 0,
    };
    getDisplayApi().overscanCalibrationAdjust(this.displayId, delta);
  }

  private resize_(x: number, y: number): void {
    const delta: Insets = {
      left: x,
      top: y,
      right: x,
      bottom: y,
    };
    getDisplayApi().overscanCalibrationAdjust(this.displayId, delta);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-display-overscan-dialog': SettingsDisplayOverscanDialogElement;
  }
}

customElements.define(
    SettingsDisplayOverscanDialogElement.is,
    SettingsDisplayOverscanDialogElement);
