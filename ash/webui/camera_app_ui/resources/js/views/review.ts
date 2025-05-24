// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import {getObjectURL} from '../models/file_system.js';
import {FileAccessEntry} from '../models/file_system_access_entry.js';
import * as nav from '../nav.js';
import {ViewName} from '../type.js';
import {instantiateTemplate, setupI18nElements} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {View} from './view.js';

interface UiArgs {
  text?: I18nString;
  label?: I18nString;
  icon?: string;
  templateId?: string;
  primary?: boolean;
}

/**
 * Available option shown in this view.
 */
export class Option<T> {
  readonly exitValue?: T;

  readonly hasPopup: boolean|null;

  readonly callback: (() => void)|null;

  /**
   * @param uiArgs Arguments to create corresponding UI.
   * @param params Handler parameters.
   * @param params.exitValue If set, the review page will exit with this value
   *     when option selected.
   * @param params.callback If set, the function get executed when option
   *     selected.
   * @param params.hasPopup Sets aria-haspopup for the option.
   */
  constructor(readonly uiArgs: UiArgs, {exitValue, callback, hasPopup}: {
    exitValue?: T,
    callback?: (() => void),
    hasPopup?: boolean,
  }) {
    this.exitValue = exitValue;
    this.hasPopup = hasPopup ?? null;
    this.callback = callback ?? null;
  }
}

/**
 * Templates to create container of options button group.
 */
export enum ButtonGroupTemplate {
  INTENT = 'review-intent-button-group-template',
  NEGATIVE = 'review-negative-button-group-template',
  POSITIVE = 'review-positive-button-group-template',
}

/**
 * Group of review options.
 */
export class OptionGroup<T> {
  readonly options: Array<Option<T>>;

  readonly template: ButtonGroupTemplate;

  constructor({options, template}:
                  {options: Array<Option<T>>, template: ButtonGroupTemplate}) {
    this.options = options;
    this.template = template;
  }
}

type ButtonGroups<T> = Array<{optionGroup: OptionGroup<T>, el: HTMLDivElement}>;

/**
 * View controller for review page.
 */
export class Review extends View {
  protected readonly image: HTMLElement;

  protected readonly video: HTMLVideoElement;

  private btnGroups: ButtonGroups<unknown> = [];

  private primaryBtn: HTMLButtonElement|null;

  constructor(private readonly viewName: ViewName = ViewName.REVIEW) {
    super(viewName, {defaultFocusSelector: '.primary', dismissByEsc: true});

    this.image = dom.getFrom(this.root, '.review-image', HTMLElement);
    this.video = dom.getFrom(this.root, '.review-video', HTMLVideoElement);
    this.primaryBtn = null;
  }

  protected async loadImage(image: HTMLImageElement, blob: Blob):
      Promise<void> {
    try {
      await new Promise<void>((resolve, reject) => {
        image.onload = () => resolve();
        image.addEventListener('error', (e) => {
          const msg = `Failed to load review document image: ${e.message}`;
          reject(new Error(msg));
        }, {once: true});
        image.src = URL.createObjectURL(blob);
      });
    } catch (e) {
      URL.revokeObjectURL(image.src);
      throw e;
    }
  }

  async setReviewPhoto(blob: Blob): Promise<void> {
    this.image.hidden = false;
    this.video.hidden = true;
    const image = assertInstanceof(this.image, HTMLImageElement);
    await this.loadImage(image, blob);
    URL.revokeObjectURL(image.src);
  }

  /**
   * Setup the video element's source for review.
   *
   * @return Function to cleanup the object URL. Make sure to call this function
   * after the review is complete.
   */
  async setReviewVideo(video: FileAccessEntry): Promise<() => void> {
    this.image.hidden = true;
    this.video.hidden = false;
    this.video.controls = true;
    const url = await getObjectURL(video);
    this.video.src = url;
    return () => {
      URL.revokeObjectURL(url);
      // When the video element's `controls` is true, the video element is
      // focusable even when it is hidden. Set `controls` to false to make it
      // not focusable. See b/301384798.
      this.video.controls = false;
    };
  }

  async startReview<T>(...optionGroups: Array<OptionGroup<T>>):
      Promise<T|null> {
    // Remove all existing button groups and buttons.
    for (const group of this.btnGroups) {
      group.el.remove();
    }
    const btnGroups: ButtonGroups<T> = [];
    this.btnGroups = btnGroups;

    // Create new button groups and buttons.
    this.primaryBtn = null;
    const onSelected = new WaitableEvent<T|null>();
    for (const group of optionGroups) {
      const templ = instantiateTemplate(`#${group.template}`);
      btnGroups.push(
          {optionGroup: group, el: dom.getFrom(templ, 'div', HTMLDivElement)});
      this.root.appendChild(templ);
    }
    for (const btnGroup of btnGroups) {
      const addButton = ({
        uiArgs: {text, label, icon, templateId, primary},
        exitValue,
        callback,
        hasPopup,
      }: Option<T|null>) => {
        const templ = instantiateTemplate(
            templateId !== undefined ? `#${templateId}` :
                                       '#text-button-template');
        const btn = dom.getFrom(templ, 'button', HTMLButtonElement);
        if (text !== undefined) {
          btn.setAttribute('i18n-text', text);
        }
        if (label !== undefined) {
          btn.setAttribute('i18n-label', label);
        }
        if (icon !== undefined) {
          const iconEl = document.createElement('svg-wrapper');
          iconEl.name = icon;
          btn.prepend(iconEl);
        }
        if (this.primaryBtn === null && primary === true) {
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
        btnGroup.el.appendChild(templ);
      };
      for (const opt of btnGroup.optionGroup.options) {
        addButton(opt);
      }
      setupI18nElements(btnGroup.el);
    }
    // The promise are indirectly awaited by waiting on onSelected.
    void nav.open(this.viewName).closed.then(() => {
      onSelected.signal(null);
    });
    const result = await onSelected.wait();
    nav.close(this.viewName);
    return result;
  }
}
