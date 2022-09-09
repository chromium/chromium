// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-guest-os-container-select' is a component enabling a
 * user to select a target container from a list stored in prefs.
 */
import 'chrome://resources/cr_elements/md_select.css.js';
import '../../settings_shared.css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerInfo, GuestId} from './guest_os_browser_proxy.js';

/**
 * @param {!GuestId} first
 * @param {!GuestId} second
 * @return boolean
 */
export function equalContainerId(first, second) {
  return first.vm_name === second.vm_name &&
      first.container_name === second.container_name;
}

/**
 * @param {!GuestId} id
 * @return string
 */
export function containerLabel(id, defaultVmName) {
  if (defaultVmName != null && id.vm_name === defaultVmName) {
    return id.container_name;
  }
  return id.vm_name + ':' + id.container_name;
}


/** @polymer */
class ContainerSelectElement extends PolymerElement {
  static get is() {
    return 'settings-guest-os-container-select';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!GuestId}
       */
      selectedContainerId: {
        type: Object,
        notify: true,
      },

      /**
       * @type {?string}
       */
      defaultVmName: {
        type: String,
        value: null,
      },

      /**
       * List of containers that are already stored in the settings.
       * @type {!Array<!ContainerInfo>}
       */
      containers: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  /**
   * @param {!Event} e
   * @private
   */
  onSelectContainer_(e) {
    const index = e.target.selectedIndex;
    if (index >= 0 && index < this.containers.length) {
      this.selectedContainerId = this.containers[index].id;
    }
  }

  /**
   * @param {!GuestId} id
   * @return string
   * @private
   */
  containerLabel_(id) {
    return containerLabel(id, this.defaultVmName);
  }
}

customElements.define(ContainerSelectElement.is, ContainerSelectElement);
