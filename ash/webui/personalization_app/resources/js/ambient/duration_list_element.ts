// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component is temporarily added to test the screen saver
 * duration settings.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {setScreenSaverDuration} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './duration_list_element.html.js';

interface DurationOption {
  name: string;
  label: string;
}

export class DurationList extends WithPersonalizationStore {
  static get is() {
    return 'duration-list';
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
        value: [
          {
            name: '5',
            label: 'for 5 minutes[temp]',
          },
          {
            name: '10',
            label: 'for 10 minutes[temp]',
          },
          {
            name: '30',
            label: 'for 30 minutes[temp]',
          },
          {
            name: '60',
            label: 'for one hour[temp]',
          },
          {
            name: '0',
            label: 'forever[temp]',
          },
        ],
      },

      selectedDuration_: {
        type: String,
        observer: 'onSelectedDurationChanged_',
      },
    };
  }

  private duration: number|null;
  private options_: DurationOption[];
  private selectedDuration_: string;

  private onDurationChanged_(value: number|null) {
    if (value) {
      this.selectedDuration_ = value.toString();
    }
  }

  private setScreenSaverDuration_(minutes: number) {
    setScreenSaverDuration(minutes, getAmbientProvider(), this.getStore());
  }

  private onSelectedDurationChanged_(value: string) {
    const minutes = parseInt(value, 10);
    if (isNaN(minutes)) {
      console.warn('Unexpected duration value received', value);
      return;
    }
    this.setScreenSaverDuration_(minutes);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'duration-list': DurationList;
  }
}

customElements.define(DurationList.is, DurationList);
