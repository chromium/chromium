// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './memory_card.js';
import './page_thumbnail.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';

import {MemoriesResult, PageCallbackRouter, PageHandlerRemote} from '/chrome/browser/ui/webui/memories/memories.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {decodeMojoString16} from './utils.js';

/**
 * @fileoverview This file provides the root custom element for the Memories
 * landing page.
 */

class MemoriesAppElement extends PolymerElement {
  static get is() {
    return 'memories-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      //========================================================================
      // Private properties
      //========================================================================

      /**
       * The currently displayed Memories returned by the browser in response to
       * a request for Memories related to a given query or those within a given
       * timespan.
       * @private {!MemoriesResult}
       */
      result_: Object,
    };
  }

  constructor() {
    super();
    /** @private {PageHandlerRemote} */
    this.pageHandler_ = BrowserProxy.getInstance().handler;
    /** @private {!PageCallbackRouter} */
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
  }

  /** @override */
  ready() {
    super.ready();
    // <if expr="not is_official_build">
    this.onBrowserIdle_().then(() => {
      const query = decodeURI(window.location.hash.substr(1));
      this.pageHandler_.getSampleMemories(query).then(({result}) => {
        this.result_ = result;
      });
    });
    // </if>
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * @param {Url} thumbnailUrl
   * @return {!{thumbnailUrl: Url}} WebPage with the thumbnailUrl property only.
   * @private
   */
  createPageWithThumbnail_(thumbnailUrl) {
    return {thumbnailUrl};
  }

  /**
   * Converts a Mojo String16 to a JS string.
   * @param {String16} str
   * @return {string}
   * @private
   */
  decodeMojoString16_(str) {
    return decodeMojoString16(str);
  }

  /**
   * @return {!Promise} A promise that resolves when the browser is idle.
   * @private
   */
  onBrowserIdle_() {
    return new Promise((resolve) => {
      window.requestIdleCallback(resolve);
    });
  }
}

customElements.define(MemoriesAppElement.is, MemoriesAppElement);
