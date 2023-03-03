// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * The zone customization dialog that allows users to customize the rgb keyboard
 * zone color.
 */

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {BacklightColor} from '../../personalization_app.mojom-webui.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {setBacklightZoneColor} from './keyboard_backlight_controller.js';
import {getKeyboardBacklightProvider} from './keyboard_backlight_interface_provider.js';
import {getTemplate} from './zone_customization_element.html.js';

export interface ZoneCustomizationElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class ZoneCustomizationElement extends WithPersonalizationStore {
  static get is() {
    return 'zone-customization';
  }

  static get template() {
    return getTemplate();
  }

  showModal() {
    this.$.dialog.showModal();
  }

  /**
   * Sets zone one color to red. TODO(b/265855838): Remove after the color
   * selector is implemented.
   */
  private setZoneOneToRed_() {
    setBacklightZoneColor(
        0, BacklightColor.kRed, getKeyboardBacklightProvider());
  }

  private closeZoneCustomizationDialog_() {
    this.$.dialog.close();
  }
}

customElements.define(ZoneCustomizationElement.is, ZoneCustomizationElement);
