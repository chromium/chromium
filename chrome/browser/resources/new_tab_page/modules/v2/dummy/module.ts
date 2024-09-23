// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '../../../strings.m.js';
import '../../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {FooDataItem} from '../../../foo.mojom-webui.js';
import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {FooProxy} from './foo_proxy.js';
import {getCss} from './module.css.js';
import {getHtml} from './module.html.js';

export interface ModuleElement {
  $: {
    tiles: HTMLElement,
  };
}

const ModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * A dummy module, which serves as an example and a helper to build out the NTP
 * module framework.
 */
export class ModuleElement extends ModuleElementBase {
  static get is() {
    return 'ntp-dummy-module';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tiles: {type: Array},
      title: {type: String},
    };
  }

  tiles: FooDataItem[];
  override title: string;

  protected onDisableButtonClick_() {
    this.fire('disable-module', {
      message: loadTimeData.getStringF(
          'disableModuleToastMessage',
          loadTimeData.getString('modulesDummyLower')),
    });
  }
}

customElements.define(ModuleElement.is, ModuleElement);

async function createDummyElements(): Promise<HTMLElement[]> {
  const tiles = (await FooProxy.getHandler().getData()).data;
  return Array(12).fill(0).map(_ => {
    const element = new ModuleElement();
    element.title = loadTimeData.getString('modulesDummyTitle');
    element.tiles = [...tiles];
    return element;
  });
}

export const dummyV2Descriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy', createDummyElements);
