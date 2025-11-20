// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for AdapterPage, served from chrome://bluetooth-internals/.
 */

import './object_fieldset.js';

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {AdapterInfo} from './adapter.mojom-webui.js';
import type {ObjectFieldsetElement} from './object_fieldset.js';
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
  adapterFieldSet: ObjectFieldsetElement;
  private refreshBtn_: HTMLButtonElement;

  constructor() {
    super('adapter', 'Adapter', 'adapter');

    this.adapterFieldSet = document.createElement('object-fieldset');
    this.adapterFieldSet.toggleAttribute('show-all', true);
    this.adapterFieldSet.dataset['nameMap'] = JSON.stringify(PROPERTY_NAMES);
    this.pageDiv.appendChild(this.adapterFieldSet);

    this.refreshBtn_ =
        getRequiredElement<HTMLButtonElement>('adapter-refresh-btn');
    this.refreshBtn_.addEventListener('click', _event => {
      this.refreshBtn_.disabled = true;
      this.pageDiv.dispatchEvent(new CustomEvent('refreshpressed'));
    });

    // <if expr="is_chromeos">
    const restartBluetoothBtn =
        getRequiredElement<HTMLButtonElement>('restart-bluetooth-btn');
    restartBluetoothBtn.addEventListener('click', () => {
      restartBluetoothBtn.disabled = true;
      this.pageDiv.dispatchEvent(new CustomEvent('restart-bluetooth-click'));
    });
    // </if>
  }

  /**
   * Sets the information to display in fieldset.
   */
  setAdapterInfo(info: AdapterInfo) {
    const infoCopy: Partial<AdapterInfo> = {...info};

    if ('systemName' in infoCopy && !infoCopy.systemName) {
      // The adapter might not implement 'systemName'. In that case, delete
      // this property so that it's not displayed on adapterFieldSet.
      delete infoCopy.systemName;
    }

    // <if expr="not is_chromeos">
    // floss and extendedAdvertisementSupport is only set in ChromeOS anyway,
    // so it's irrelevant on other platforms. Delete them.
    if ('floss' in infoCopy) {
      delete infoCopy.floss;
    }
    if ('extendedAdvertisementSupport' in infoCopy) {
      delete infoCopy.extendedAdvertisementSupport;
    }
    // </if>

    this.adapterFieldSet.dataset['value'] = JSON.stringify(infoCopy);
    this.refreshBtn_.disabled = false;
  }
}
