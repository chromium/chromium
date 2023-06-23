// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../../strings.m.js';
import '../../module_header.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FooDataItem} from '../../../foo.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {FooProxy} from './foo_proxy.js';
import {getTemplate} from './module.html.js';

export interface DummyModuleElement {
  $: {
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

async function createDummyElements(): Promise<HTMLElement[]|null> {
  const tiles = (await FooProxy.getHandler().getData()).data;
  return (Array(12).fill(0).map(_ => {
           const element = new DummyModuleElement();
           element.title = loadTimeData.getString('modulesDummyTitle');
           element.tiles = [...tiles];
           return element;
         }) as unknown) as HTMLElement[];
}

export const dummyV2Descriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy', createDummyElements);
