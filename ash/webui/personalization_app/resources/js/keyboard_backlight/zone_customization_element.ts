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

import {WithPersonalizationStore} from '../personalization_store.js';

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

  private closeZoneCustomizationDialog_() {
    this.$.dialog.close();
  }
}

customElements.define(ZoneCustomizationElement.is, ZoneCustomizationElement);
