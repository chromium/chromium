// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../../strings.m.js';
import '../../module_header.js';

import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FooDataItem} from '../../../foo.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {FooProxy} from './foo_proxy.js';
import {getTemplate} from './module.html.js';

export interface DummyModuleElement {
  $: {
    tileList: DomRepeat,
    tiles: HTMLElement,
  };
}

/**
 * A dummy module, which serves as an example and a helper to build out the NTP
 * module framework.
 */
export class DummyModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-dummy-module';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tiles: Array,
      title: String,
    };
  }

  tiles: FooDataItem[];
  override title: string;

  constructor() {
    super();
    this.initializeData_();
  }

  private async initializeData_() {
    const tileData = await FooProxy.getHandler().getData();
    this.tiles = tileData.data;
  }

  private onDisableButtonClick_() {
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

function createDummyElement(titleId: string): DummyModuleElement {
  const element = new DummyModuleElement();
  element.title = loadTimeData.getString(titleId);
  return element;
}

export const dummyV2Descriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy',
    () => Promise.resolve(createDummyElement('modulesDummyTitle')));

export const dummyV2Descriptor02: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy2',
    () => Promise.resolve(createDummyElement('modulesDummy2Title')));

export const dummyV2Descriptor03: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy3',
    () => Promise.resolve(createDummyElement('modulesDummy3Title')));

export const dummyV2Descriptor04: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy4',
    () => Promise.resolve(createDummyElement('modulesDummy4Title')));

export const dummyV2Descriptor05: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy5',
    () => Promise.resolve(createDummyElement('modulesDummy5Title')));

export const dummyV2Descriptor06: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy6',
    () => Promise.resolve(createDummyElement('modulesDummy6Title')));

export const dummyV2Descriptor07: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy7',
    () => Promise.resolve(createDummyElement('modulesDummy7Title')));

export const dummyV2Descriptor08: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy8',
    () => Promise.resolve(createDummyElement('modulesDummy8Title')));

export const dummyV2Descriptor09: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy9',
    () => Promise.resolve(createDummyElement('modulesDummy9Title')));

export const dummyV2Descriptor10: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy10',
    () => Promise.resolve(createDummyElement('modulesDummy10Title')));

export const dummyV2Descriptor11: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy11',
    () => Promise.resolve(createDummyElement('modulesDummy11Title')));

export const dummyV2Descriptor12: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy12',
    () => Promise.resolve(createDummyElement('modulesDummy12Title')));
