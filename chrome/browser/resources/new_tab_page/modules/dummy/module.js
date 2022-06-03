// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../strings.m.js';
import '../module_header.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {FooProxy} from './foo_proxy.js';

/**
 * A dummy module, which serves as an example and a helper to build out the NTP
 * module framework.
 * @polymer
 * @extends {PolymerElement}
 */
class DummyModuleElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-dummy-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!Array<!foo.mojom.FooDataItem>} */
      tiles: Array,

      /** @type {!string} */
      title: String,
    };
  }

  constructor() {
    super();
    this.initializeData_();
  }

  /** @private */
  async initializeData_() {
    const tileData = await FooProxy.getHandler().getData();
    this.tiles = tileData.data;
  }

  /** @private */
  onDisableButtonClick_() {
    this.dispatchEvent(new CustomEvent('disable-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesDummyLower')),
      },
    }));
  }
}

customElements.define(DummyModuleElement.is, DummyModuleElement);

/**
 * @param {!string} titleId
 * @return {!DummyModuleElement}
 */
function createDummyElement(titleId) {
  const element = new DummyModuleElement();
  element.title = loadTimeData.getString(titleId);
  return element;
}

/** @type {!ModuleDescriptor} */
export const dummyDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy',
    /*name=*/ loadTimeData.getString('modulesDummyTitle'),
    () => Promise.resolve(createDummyElement('modulesDummyTitle')));

/** @type {!ModuleDescriptor} */
export const dummyDescriptor2 = new ModuleDescriptor(
    /*id=*/ 'dummy2',
    /*name=*/ loadTimeData.getString('modulesDummy2Title'),
    () => Promise.resolve(createDummyElement('modulesDummy2Title')));
