// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that presents ambient duration settings in
 * the ambient subpage element.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {setScreenSaverDuration} from './ambient_controller.js';
import {getTemplate} from './ambient_duration_element.html.js';
import {getAmbientProvider} from './ambient_interface_provider.js';

export class AmbientDurationElement extends WithPersonalizationStore {
  static get is() {
    return 'ambient-duration';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used to refer to the enum values in HTML file.
       */
      duration: {
        type: Number,
        observer: 'onDurationChanged_',
      },

      options_: {
        type: Array,
        value: ['5', '10', '30', '60', '0'],
      },

      selectedDuration_: {
        type: String,
        observer: 'onSelectedDurationChanged_',
      },
    };
  }

  private duration: number|null;
  private options_: string[];
  private selectedDuration_: string;

  private getDurationLabel_(name: string): string {
    switch (name) {
      case '5':
        return this.i18n('ambientModeDurationMinutes', 5);
      case '10':
        return this.i18n('ambientModeDurationMinutes', 10);
      case '30':
        return this.i18n('ambientModeDurationMinutes', 30);
      case '60':
        return this.i18n('ambientModeDurationOneHour');
      case '0':
        return this.i18n('ambientModeDurationForever');
      default:
        console.error('Unknown screen saver duration value.');
        return '';
    }
  }

  private onDurationChanged_(value: number|null) {
    if (typeof value == 'number') {
      this.selectedDuration_ = value.toString();
    }
  }

  private onOptionChanged_(): void {
    const elem: HTMLSelectElement|null =
        this.shadowRoot!.querySelector('#durationOptions');
    if (elem) {
      this.selectedDuration_ = elem.value;
    }
  }

  private setScreenSaverDuration_(minutes: number): void {
    setScreenSaverDuration(minutes, getAmbientProvider(), this.getStore());
  }

  private onSelectedDurationChanged_(value: string): void {
    const minutes = parseInt(value, 10);
    if (isNaN(minutes)) {
      console.warn('Unexpected duration value received', value);
      return;
    }
    this.setScreenSaverDuration_(minutes);
  }

  private isEqual_(lhs: string, rhs: string): boolean {
    return lhs === rhs;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ambient-duration': AmbientDurationElement;
  }
}

customElements.define(AmbientDurationElement.is, AmbientDurationElement);
