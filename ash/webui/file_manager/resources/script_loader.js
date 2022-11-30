// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script loader allows loading scripts at the desired time.
 */

/**
 * @typedef {
 *   {type: (string|undefined), defer: (boolean|undefined)}
 * }
 */
export let ScriptParams;


/**
 * Used to load scripts at a runtime. Typical use:
 *
 * await new ScriptLoader('its_time.js').load();
 *
 * Optional parameters may be also specified:
 *
 * await new ScriptLoader('its_time.js', {type: 'module'}).load();
 */
export class ScriptLoader {
  /**
   * Creates a loader that loads the script specified by |src| once the load
   * method is called. Optional |params| can specify other script attributes.
   * @param {string} src
   * @param {!ScriptParams} params
   */
  constructor(src, params = {}) {
    /** @private {string} */
    this.src_ = src;

    /** @private {string|undefined} */
    this.type_ = params.type;

    /** @private {boolean|undefined} */
    this.defer_ = params.defer;
  }

  /**
   * @return {!Promise<string>}
   */
  async load() {
    return new Promise((resolve, reject) => {
      const script = document.createElement('script');
      if (this.type_ !== undefined) {
        script.type = this.type_;
      }
      if (this.defer_ !== undefined) {
        script.defer = this.defer_;
      }
      script.onload = () => resolve(this.src_);
      script.onerror = (error) => reject(error);
      script.src = this.src_;
      document.head.append(script);
    });
  }
}
