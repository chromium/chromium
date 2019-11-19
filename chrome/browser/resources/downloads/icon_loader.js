// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getFileIconUrl} from 'chrome://resources/js/icon.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

  export class IconLoader {
    constructor() {
      /** @private {!Object<!PromiseResolver<boolean>>} */
      this.iconResolvers_ = {};

      /** @private {!Set<!HTMLImageElement>} */
      this.listeningImages_ = new Set();
    }

    /**
     * @param {!HTMLImageElement} imageEl
     * @param {string} filePath
     * @return {!Promise<boolean>} Whether or not the icon loaded successfully.
     */
    loadIcon(imageEl, filePath) {
      const url = getFileIconUrl(filePath);

      if (!this.iconResolvers_[url]) {
        this.iconResolvers_[url] = new PromiseResolver();
      }

      if (!this.listeningImages_.has(imageEl)) {
        imageEl.addEventListener('load', this.finishedLoading_.bind(this));
        imageEl.addEventListener('error', this.finishedLoading_.bind(this));
        this.listeningImages_.add(imageEl);
      }

      imageEl.src = url;

      return assert(this.iconResolvers_[url]).promise;
    }

    /**
     * @param {!Event} e
     * @private
     */
    finishedLoading_(e) {
      const resolver = assert(this.iconResolvers_[e.currentTarget.src]);
      if (!resolver.isFulfilled) {
        resolver.resolve(e.type == 'load');
      }
    }
  }

  addSingletonGetter(IconLoader);
