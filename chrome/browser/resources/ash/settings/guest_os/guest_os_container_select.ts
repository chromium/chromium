// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-guest-os-container-select' is a component enabling a
 * user to select a target container from a list stored in prefs.
 */
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {cast} from '../assert_extras.js';

import {ContainerInfo, GuestId} from './guest_os_browser_proxy.js';
import {getTemplate} from './guest_os_container_select.html.js';

export function equalContainerId(first: GuestId, second: GuestId): boolean {
  return first.vm_name === second.vm_name &&
      first.container_name === second.container_name;
}

export function containerLabel(
    id: GuestId, defaultVmName: string|null): string {
  if (defaultVmName !== null && id.vm_name === defaultVmName) {
    return id.container_name;
  }
  return id.vm_name + ':' + id.container_name;
}

export class ContainerSelectElement extends PolymerElement {
  static get is() {
    return 'settings-guest-os-container-select';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedContainerId: {
        type: Object,
        notify: true,
      },

      defaultVmName: {
        type: String,
        value: null,
      },

      /**
       * List of containers that are already stored in the settings.
       */
      containers: {
        type: Array,
        value() {
          return [];
        },
      },
    };
  }

  selectedContainerId: GuestId;
  defaultVmName: string|null;
  containers: ContainerInfo[];

  private onSelectContainer_(e: Event): void {
    const index = cast(e.target, HTMLSelectElement).selectedIndex;
    if (index >= 0 && index < this.containers.length) {
      this.selectedContainerId = this.containers[index].id;
    }
  }

  private containerLabel_(id: GuestId): string {
    return containerLabel(id, this.defaultVmName);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-guest-os-container-select': ContainerSelectElement;
  }
}

customElements.define(ContainerSelectElement.is, ContainerSelectElement);
