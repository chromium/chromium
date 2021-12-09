// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import * as nav from '../nav.js';
import {ViewName} from '../type.js';
import {instantiateTemplate, setupI18nElements} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {View} from './view.js';

/**
 * Available option show in this view.
 */
export class Option<T> {
  readonly exitValue: T|null|undefined;
  readonly hasPopup: boolean|null;
  readonly callback: (() => void)|null;
  /**
   * @param text Text string show on the option button.
   * @param handlerParams Sets |exitValue| if the review page will exit with
   *     this value when option selected. Sets |callback| for the function get
   *     executed when option selected.
   */
  constructor(readonly text: I18nString, {exitValue, callback, hasPopup}: {
    exitValue?: (T|null);
    callback?: (() => void);
    hasPopup?: boolean;
  }) {
    this.exitValue = exitValue;
    this.hasPopup = hasPopup ?? null;
    this.callback = callback || null;
  }
}

/**
 * Options for reviewing.
 */
export class Options<T> {
  readonly options: Option<T|null>[];

  /** Constructs Options. */
  constructor(...options: Option<T|null>[]) {
    this.options = options;
  }
}

/**
 * View controller for review page.
 */
export class Review extends View {
  protected readonly image: HTMLElement;
  private readonly positiveBtns: HTMLDivElement;
  private readonly negativeBtns: HTMLDivElement;
  private primaryBtn: HTMLButtonElement|null;

  /**
   * Constructs the review view.
   */
  constructor(private readonly viewName: ViewName = ViewName.REVIEW) {
    super(viewName, {defaultFocusSelector: '.primary'});

    this.image = dom.getFrom(this.root, '.review-image', HTMLElement);
    this.positiveBtns =
        dom.getFrom(this.root, '.positive.button-group', HTMLDivElement);
    this.negativeBtns =
        dom.getFrom(this.root, '.negative.button-group', HTMLDivElement);
    this.primaryBtn = null;
  }

  /**
   * Load the image element with given blob.
   */
  protected async loadImage(image: HTMLImageElement, blob: Blob):
      Promise<void> {
    try {
      await new Promise<void>((resolve, reject) => {
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
   * Sets the photo to be reviewed.
   */
  async setReviewPhoto(blob: Blob): Promise<void> {
    const image = assertInstanceof(this.image, HTMLImageElement);
    await this.loadImage(image, blob);
    URL.revokeObjectURL(image.src);
  }

  /**
   * Starts review.
   */
  async startReview<T>({positive, negative}: {
    positive: Options<T|null>; negative: Options<T|null>;
  }): Promise<T|null> {
    // Remove all existing buttons.
    for (const btnGroup of [this.positiveBtns, this.negativeBtns]) {
      while (btnGroup.lastChild !== null) {
        btnGroup.removeChild(btnGroup.lastChild);
      }
    }

    this.primaryBtn = null;
    const onSelected = new WaitableEvent<T|null>();
    for (const btnGroup of [this.positiveBtns, this.negativeBtns]) {
      const options = (btnGroup === this.positiveBtns ? positive : negative);
      const addButton =
          ({text, exitValue, callback, hasPopup}: Option<T|null>) => {
            const templ = instantiateTemplate('#text-button-template');
            const btn = dom.getFrom(templ, 'button', HTMLButtonElement);
            btn.setAttribute('i18n-text', text);
            if (this.primaryBtn === null) {
              btn.classList.add('primary');
              this.primaryBtn = btn;
            } else {
              btn.classList.add('secondary');
            }
            if (hasPopup !== null) {
              btn.setAttribute('aria-haspopup', hasPopup.toString());
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

    nav.open(this.viewName);
    const result = await onSelected.wait();
    nav.close(this.viewName);
    return result;
  }
}
