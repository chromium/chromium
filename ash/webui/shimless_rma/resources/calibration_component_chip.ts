// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './calibration_component_chip.html.js';
import {CLICK_CALIBRATION_COMPONENT_BUTTON, ClickCalibrationComponentEvent} from './events.js';
import {modifyTabbableElement} from './shimless_rma_util.js';

declare global {
  interface HTMLElementEventMap {
    [CLICK_CALIBRATION_COMPONENT_BUTTON]: ClickCalibrationComponentEvent;
  }
}

/**
 * @fileoverview
 * 'calibration-component-chip' represents a single component chip that reports
 * status of last calibration attempt and can be marked to skip.
 */

export class CalibrationComponentChipElement extends PolymerElement {
  static get is() {
    return 'calibration-component-chip' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      failed: {type: Boolean, value: false},

      componentName: {type: String, value: ''},

      disabled: {
        type: Boolean,
        value: false,
      },

      isFirstClickableComponent: {
        type: Boolean,
        value: false,
        observer: CalibrationComponentChipElement.prototype
                      .onIsFirstClickableComponentChanged,
      },

      uniqueId: {
        reflectToAttribute: true,
        type: Number,
        value: '',
      },
    };
  }

  checked: boolean;
  failed: boolean;
  componentName: string;
  disabled: boolean;
  isFirstClickableComponent: boolean;
  uniqueId: number;

  protected onComponentButtonClicked() {
    this.checked = !this.checked;

    // Notify the page that the component chip was clicked, so that the page can
    // put the focus on it.
    this.dispatchEvent(new CustomEvent<number>('click-calibration-component-button', {
      bubbles: true,
      composed: true,
      detail: this.uniqueId,
    }));
  }

  override click() {
    this.onComponentButtonClicked();
  }

  /**
   * Show the checked icon for disabled calibration components because if it's
   * disabled, that means it alerady passed calibration.
   */
  protected shouldShowCheckIcon(): boolean {
    return this.checked || this.disabled;
  }

  private onIsFirstClickableComponentChanged() {
    // Tab should go to the first non-disabled component in the list,
    // not individual component.
    modifyTabbableElement(
        this.shadowRoot!.querySelector<HTMLElement>('#componentButton')!,
        this.isFirstClickableComponent);
  }

  protected isAriaPressed(): string {
    return this.checked.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CalibrationComponentChipElement.is]: CalibrationComponentChipElement;
  }
}

customElements.define(
    CalibrationComponentChipElement.is, CalibrationComponentChipElement);
