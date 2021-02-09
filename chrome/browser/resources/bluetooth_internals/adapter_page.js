// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for AdapterPage, served from chrome://bluetooth-internals/.
 */

import {$} from 'chrome://resources/js/util.m.js';
import {ObjectFieldSet} from './object_fieldset.js';
import {Page} from './page.js';

const PROPERTY_NAMES = {
  address: 'Address',
  name: 'Name',
  systemName: 'System Name',
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

    this.adapterFieldSet = new ObjectFieldSet();
    this.adapterFieldSet.setPropertyDisplayNames(PROPERTY_NAMES);
    this.pageDiv.appendChild(this.adapterFieldSet);

    this.refreshBtn_ = $('adapter-refresh-btn');
    this.refreshBtn_.addEventListener('click', event => {
      this.refreshBtn_.disabled = true;
      this.pageDiv.dispatchEvent(new CustomEvent('refreshpressed'));
    });
  }

  /**
   * Sets the information to display in fieldset.
   * @param {!bluetooth.mojom.AdapterInfo} info
   */
  setAdapterInfo(info) {
    if (info.hasOwnProperty('systemName') && !info.systemName) {
      // The adapter might not implement 'systemName'. In that case, delete
      // this property so that it's not displayed on adapterFieldSet.
      delete info.systemName;
    }

    this.adapterFieldSet.setObject(info);
    this.refreshBtn_.disabled = false;
  }

  /**
   * Redraws the fieldset displaying the adapter info.
   */
  redraw() {
    this.adapterFieldSet.redraw();
    this.refreshBtn_.disabled = false;
  }
}
