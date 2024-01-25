// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for AdapterPage, served from chrome://bluetooth-internals/.
 */

import './object_fieldset.js';

import {$} from 'chrome://resources/js/util.js';

import {Page} from './page.js';

const PROPERTY_NAMES = {
  address: 'Address',
  name: 'Name',
  systemName: 'System Name',
  floss: 'Floss',
  initialized: 'Initialized',
  present: 'Present',
  powered: 'Powered',
  discoverable: 'Discoverable',
  discovering: 'Discovering',
};

/**
 * Page that contains an ObjectFieldSet that displays the latest AdapterInfo.
 */
export class AdapterPage extends Page {
  constructor() {
    super('adapter', 'Adapter', 'adapter');

    this.adapterFieldSet = document.createElement('object-field-set');
    this.adapterFieldSet.toggleAttribute('show-all', true);
    this.adapterFieldSet.dataset.nameMap = JSON.stringify(PROPERTY_NAMES);
    this.pageDiv.appendChild(this.adapterFieldSet);

    this.refreshBtn_ = $('adapter-refresh-btn');
    this.refreshBtn_.addEventListener('click', event => {
      this.refreshBtn_.disabled = true;
      this.pageDiv.dispatchEvent(new CustomEvent('refreshpressed'));
    });

    // <if expr="chromeos_ash">
    const restartBluetoothBtn = $('restart-bluetooth-btn');
    restartBluetoothBtn.addEventListener('click', () => {
      restartBluetoothBtn.disabled = true;
      this.pageDiv.dispatchEvent(new CustomEvent('restart-bluetooth-click'));
    });
    // </if>
  }

  /**
   * Sets the information to display in fieldset.
   * @param {!AdapterInfo} info
   */
  setAdapterInfo(info) {
    if (info.hasOwnProperty('systemName') && !info.systemName) {
      // The adapter might not implement 'systemName'. In that case, delete
      // this property so that it's not displayed on adapterFieldSet.
      delete info.systemName;
    }

    this.adapterFieldSet.dataset.value = JSON.stringify(info);
    this.refreshBtn_.disabled = false;
  }
}
