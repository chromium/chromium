// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';

import '../../img.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ModuleDescriptor} from '../module_descriptor.js';

/**
 * @fileoverview A dummy module, which serves as an example and a helper to
 * build out the NTP module framework.
 */

class DummyModuleElement extends PolymerElement {
  static get is() {
    return 'ntp-dummy-module';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      tiles: {
        type: Array,
        value: () => ([
          {
            label: 'item1',
            value: 'foo',
            imageUrl: 'https://lh4.googleusercontent.com/proxy/' +
                'kFIJNnm2DMbS3B5LXaIdm2JKI6twGWwmzQbcJCfqTfuaH_' +
                'ULD50v1Z3BGPEF32xTPRvgGLx492zcy_kcatCde2wmz-9Z' +
                'YFqifbJRMl2DzyE=w170-h85-p-k-no-nd-mv',
          },
          {
            label: 'item2',
            value: 'bar',
            imageUrl: 'https://lh6.googleusercontent.com/proxy/' +
                'KyyCsF6dIQ783r3Znmvdo76QY2RgzcR5t4rnA5kKjsmrlp' +
                'sb_pWGndQkyuAI4mv68X_9ZX2Edd-0FP4iQZRFm8UAW3oD' +
                'X8Coqk3C85UNAX3H4Eh_5wGyDB0SY6HOQjOXVQ=w170-h85-p-k-no-nd-mv',
          },
          {
            label: 'item3',
            value: 'baz',
            imageUrl: 'https://lh6.googleusercontent.com/proxy/' +
                '4IP40Q18w6aDF4oS4WRnUj0MlCCKPK-vLHqSd4r-RfS6Jx' +
                'gblG5WJuRYpkJkoTzLMS0qv3Sxhf9wdaKkn3vHnyy6oe7Ah' +
                '5y0=w170-h85-p-k-no-nd-mv',
          },
          {
            label: 'item4',
            value: 'foo',
            imageUrl: 'https://lh3.googleusercontent.com/proxy/' +
                'd_4gDNBtm9Ddv8zqqm0MVY93_j-_e5M-bGgH-bSAfIR65F' +
                'YGacJTemvNp9fDT0eiIbi3bzrf7HMMsupe2QIIfm5H7BMH' +
                'Y3AI5rkYUpx-lQ=w170-h85-p-k-no-nd-mv',
          },
          {
            label: 'item5',
            value: 'bar',
            imageUrl: 'https://lh5.googleusercontent.com/proxy/' +
                'xvtq6_782kBajCBr0GISHpujOb51XLKUeEOJ2lLPKh12-x' +
                'NBTCtsoHT14NQcaH9l4JhatcXEMBkqgUeCWhb3XhdLnD1B' +
                'iNzQ_LVydwg=w170-h85-p-k-no-nd-mv',
          },
          {
            label: 'item6',
            value: 'baz',
            imageUrl: 'https://lh6.googleusercontent.com/proxy/' +
                'fUx750lchxFJb3f37v_-4iJPzcTKtJbd5LDRO7S9Xy7nkP' +
                'zh7HFU61tN36j4Diaa9Yk3K7kWshRwmqcrulnhbeJrRpIn' +
                '79PjHN-N=w170-h85-p-k-no-nd-mv',
          },
        ]),
      }
    };
  }
}

customElements.define(DummyModuleElement.is, DummyModuleElement);

/** @type {!ModuleDescriptor} */
export const dummyDescriptor = new ModuleDescriptor(
    /*id=*/ 'dummy',
    /*heightPx=*/ 260, () => Promise.resolve({
      element: new DummyModuleElement(),
      title: loadTimeData.getString('modulesDummyTitle'),
    }));

/** @type {!ModuleDescriptor} */
export const dummyDescriptor2 = new ModuleDescriptor(
    /*id=*/ 'dummy2',
    /*heightPx=*/ 260, () => Promise.resolve({
      element: new DummyModuleElement(),
      title: loadTimeData.getString('modulesDummy2Title'),
    }));
