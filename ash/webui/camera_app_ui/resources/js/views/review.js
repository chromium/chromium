// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../chrome_util.js';
import * as dom from '../dom.js';
// eslint-disable-next-line no-unused-vars
import {I18nString} from '../i18n_string.js';
import * as nav from '../nav.js';
import {ViewName} from '../type.js';
import {instantiateTemplate, setupI18nElements} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {View} from './view.js';

/**
 * Available option show in this view.
 * @template T
 */
export class Option {
  /**
   * @param {!I18nString} text Text string show on the option button.
   * @param {{
   *   exitValue: (?T|undefined),
   *   callback: (function()|undefined),
   *   hasPopup: (boolean|undefined),
   * }} handlerParams Sets |exitValue| if the review page will exit with this
   *   value when option selected. Sets |callback| for the function get executed
   *   when option selected.
   */
  constructor(text, {exitValue, callback, hasPopup}) {
    /**
     * @const {!I18nString}
     */
    this.text = text;

    /**
     * @const {(?T|undefined)}
     */
    this.exitValue = exitValue;

    /**
     * @const {?boolean}
     */
    this.hasPopup = hasPopup ?? null;

    /**
     * @const {?function()}
     */
    this.callback = callback || null;
  }
}

/**
 * Options for reviewing.
 * @template T
 */
export class Options {
  /**
   * @param {...!Option<?T>} options
   */
  constructor(...options) {
    /**
     * @const {!Array<!Option<?T>>}
     */
    this.options = options;
  }
}

/**
 * View controller for review page.
 */
export class Review extends View {
  /**
   * @param {!ViewName=} viewName
   * @public
   */
  constructor(viewName = ViewName.REVIEW) {
    super(viewName);

    /**
     * @private {!ViewName}
     * @const
     */
    this.viewName_ = viewName;

    /**
     * @protected {!HTMLElement}
     * @const
     */
    this.image_ = dom.getFrom(this.root, '.review-image', HTMLElement);

    /**
     * @private {!HTMLDivElement}
     * @const
     */
    this.positiveBtns_ =
        dom.getFrom(this.root, '.positive.button-group', HTMLDivElement);

    /**
     * @private {!HTMLDivElement}
     * @const
     */
    this.negativeBtns_ =
        dom.getFrom(this.root, '.negative.button-group', HTMLDivElement);

    /**
     * @private {?HTMLButtonElement}
     */
    this.primaryBtn_ = null;
  }

  /**
   * @override
   */
  focus() {
    this.primaryBtn_.focus();
  }

  /**
   * @param {!Image|!HTMLImageElement} image
   * @param {!Blob} blob
   * @protected
   */
  async loadImage_(image, blob) {
    try {
      await new Promise((resolve, reject) => {
        image.onload = () => resolve();
        image.onerror = (e) =>
            reject(new Error(`Failed to load review document image: ${e}`));
        image.src = URL.createObjectURL(blob);
      });
    } catch (e) {
      URL.revokeObjectURL(image.src);
      throw e;
    }
  }

  /**
   * @param {!Blob} blob
   * @return {!Promise}
   */
  async setReviewPhoto(blob) {
    const image = assertInstanceof(this.image_, HTMLImageElement);
    await this.loadImage_(image, blob);
    URL.revokeObjectURL(image.src);
  }

  /**
   * @template T
   * @param {{positive: !Options<?T>, negative: !Options<?T>}} options
   * @return {!Promise<?T>}
   */
  async startReview({positive, negative}) {
    // Remove all existing buttons.
    for (const btnGroup of [this.positiveBtns_, this.negativeBtns_]) {
      while (btnGroup.firstChild) {
        btnGroup.removeChild(btnGroup.lastChild);
      }
    }

    this.primaryBtn_ = null;
    const onSelected = new WaitableEvent();
    for (const btnGroup of [this.positiveBtns_, this.negativeBtns_]) {
      const options = (btnGroup === this.positiveBtns_ ? positive : negative);
      /**
       * @param {!Option<?T>} option
       */
      const addButton = ({text, exitValue, callback, hasPopup}) => {
        const templ = instantiateTemplate('#text-button-template');
        const btn = dom.getFrom(templ, 'button', HTMLButtonElement);
        btn.setAttribute('i18n-text', text);
        if (this.primaryBtn_ === null) {
          btn.classList.add('primary');
          this.primaryBtn_ = btn;
        } else {
          btn.classList.add('secondary');
        }
        if (hasPopup !== null) {
          btn.setAttribute('aria-haspopup', hasPopup);
        }
        btn.onclick = () => {
          if (callback !== null) {
            callback();
          }
          if (exitValue !== undefined) {
            onSelected.signal(exitValue);
          }
        };
        btnGroup.appendChild(templ);
      };
      for (const opt of options.options) {
        addButton(opt);
      }
      setupI18nElements(btnGroup);
    }

    nav.open(this.viewName_);
    const result = await onSelected.wait();
    nav.close(this.viewName_);
    return result;
  }
}
