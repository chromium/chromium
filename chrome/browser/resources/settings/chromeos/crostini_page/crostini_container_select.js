// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-container-select' is a component enabling a
 * user to select a target container from a list stored in prefs.
 */
import '//resources/cr_elements/md_select_css.m.js';
import '../../settings_shared_css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContainerId, ContainerInfo, DEFAULT_CONTAINER_ID, DEFAULT_CROSTINI_VM} from './crostini_browser_proxy.js';

/**
 * @param {!ContainerId} first
 * @param {!ContainerId} second
 * @return boolean
 */
export function equalContainerId(first, second) {
  return first.vm_name === second.vm_name &&
      first.container_name === second.container_name;
}

/**
 * @param {!ContainerId} id
 * @return string
 */
export function containerLabel(id) {
  if (id.vm_name === DEFAULT_CROSTINI_VM) {
    return id.container_name;
  }
  return id.vm_name + ':' + id.container_name;
}


/** @polymer */
class ContainerSelectElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-container-select';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!ContainerId}
       */
      selectedContainerId: {
        type: Object,
        notify: true,
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
   * @param {!ContainerId} id
   * @return string
   * @private
   */
  containerLabel_(id) {
    return containerLabel(id);
  }
}

customElements.define(ContainerSelectElement.is, ContainerSelectElement);
