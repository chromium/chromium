// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../strings.m.js';
import '../module_header.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FooDataItem} from '../../foo.mojom-webui.js';
import {I18nBehavior, loadTimeData} from '../../i18n_setup.js';
import {ModuleDescriptorV2, ModuleHeight} from '../module_descriptor.js';

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
      /** @type {!Array<!FooDataItem>} */
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

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor = new ModuleDescriptorV2(
    /*id=*/ 'dummy',
    /*name=*/ loadTimeData.getString('modulesDummyTitle'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummyTitle')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor02 = new ModuleDescriptorV2(
    /*id=*/ 'dummy2',
    /*name=*/ loadTimeData.getString('modulesDummy2Title'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummy2Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor03 = new ModuleDescriptorV2(
    /*id=*/ 'dummy3',
    /*name=*/ loadTimeData.getString('modulesDummy3Title'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummy3Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor04 = new ModuleDescriptorV2(
    /*id=*/ 'dummy4',
    /*name=*/ loadTimeData.getString('modulesDummy4Title'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummy4Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor05 = new ModuleDescriptorV2(
    /*id=*/ 'dummy5',
    /*name=*/ loadTimeData.getString('modulesDummy5Title'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummy5Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor06 = new ModuleDescriptorV2(
    /*id=*/ 'dummy6',
    /*name=*/ loadTimeData.getString('modulesDummy6Title'),
    /*height*/ ModuleHeight.SHORT,
    () => Promise.resolve(createDummyElement('modulesDummy6Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor07 = new ModuleDescriptorV2(
    /*id=*/ 'dummy7',
    /*name=*/ loadTimeData.getString('modulesDummy7Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy7Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor08 = new ModuleDescriptorV2(
    /*id=*/ 'dummy8',
    /*name=*/ loadTimeData.getString('modulesDummy8Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy8Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor09 = new ModuleDescriptorV2(
    /*id=*/ 'dummy9',
    /*name=*/ loadTimeData.getString('modulesDummy9Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy9Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor10 = new ModuleDescriptorV2(
    /*id=*/ 'dummy10',
    /*name=*/ loadTimeData.getString('modulesDummy10Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy10Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor11 = new ModuleDescriptorV2(
    /*id=*/ 'dummy11',
    /*name=*/ loadTimeData.getString('modulesDummy11Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy11Title')));

/** @type {!ModuleDescriptorV2} */
export const dummyV2Descriptor12 = new ModuleDescriptorV2(
    /*id=*/ 'dummy12',
    /*name=*/ loadTimeData.getString('modulesDummy12Title'),
    /*height*/ ModuleHeight.TALL,
    () => Promise.resolve(createDummyElement('modulesDummy12Title')));
