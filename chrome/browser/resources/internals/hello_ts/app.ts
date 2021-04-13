// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {helloWorld} from 'chrome://resources/js/hello_world.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class TsAppElement extends PolymerElement {
  static get is() {
    return 'ts-app';
  }

  static get template() {
    return html`
      <div>[[message]]</div>
      <iron-pages attr-for-selected="id" selected="page0">
        <div id="page0">Page0</div>
        <div id="page1">Page1</div>
      </iron-pages>
    `;
  }

  static get properties() {
    return {
      message: String,
    };
  }

  private message: string;

  constructor() {
    super();

    // Try a third_party/polymer dependency. Ensure that TypeScript infers
    // correctly the type from document.createElement() without explicitly
    // declaring IronPagesElement as the type.
    const ironPages = document.createElement('iron-pages');
    ironPages.notifyResize();

    // Try helloWorld().
    this.message = helloWorld() + ' from TypeScript!';

    // <if expr="chromeos">
    this.message = helloWorld() + ' from CrOS TypeScript!';
    // </if>

    // Try assert, getDeepActiveElement.
    assert(getDeepActiveElement() === document.body);

    // Try PromiseResolver with a string.
    const stringResolver = new PromiseResolver<string>();
    stringResolver.promise.then((result: string) => {
      assert(result.includes('done'));
    });
    stringResolver.resolve('done');

    // Try PromiseResolver with a number.
    const numberResolver = new PromiseResolver<number>();
    numberResolver.promise.then((result: number) => {
      assert(Math.min(result, 0) === 0);
    });
    numberResolver.resolve(5);
  }

  ready() {
    super.ready();
    console.log(this.message);
  }

  connectedCallback() {
    super.connectedCallback();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
  }
}

customElements.define(TsAppElement.is, TsAppElement);
