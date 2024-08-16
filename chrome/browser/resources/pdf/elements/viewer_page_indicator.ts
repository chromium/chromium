// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Viewport} from '../viewport.js';

import {getCss} from './viewer_page_indicator.css.js';
import {getHtml} from './viewer_page_indicator.html.js';

export interface ViewerPageIndicatorElement {
  $: {
    text: HTMLElement,
  };
}

export class ViewerPageIndicatorElement extends CrLitElement {
  static get is() {
    return 'viewer-page-indicator';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      index: {type: Number},
      pageLabels: {type: Array},
    };
  }

  index: number = 0;
  pageLabels: number[]|null = null;
  private timerId_?: number;
  private viewport_: Viewport|null = null;

  override firstUpdated() {
    const callback = () => this.fadeIn_();
    window.addEventListener('scroll', function() {
      requestAnimationFrame(callback);
    });
  }

  setViewport(viewport: Viewport) {
    this.viewport_ = viewport;
  }

  private fadeIn_() {
    // Vertically position relative to scroll position.
    let percent = 0;
    if (this.viewport_) {
      percent = this.viewport_.position.y /
          (this.viewport_.contentSize.height - this.viewport_.size.height);
    }
    this.style.top =
        percent * (document.documentElement.clientHeight - this.offsetHeight) +
        'px';

    // Horizontally position to compensate for overlay scrollbars.
    assert(document.documentElement.dir);
    let overlayScrollbarWidth = 0;
    if (this.viewport_ && this.viewport_.documentHasScrollbars().vertical) {
      overlayScrollbarWidth = this.viewport_.overlayScrollbarWidth;
    }
    this.style[isRTL() ? 'left' : 'right'] = `${overlayScrollbarWidth}px`;

    // Animate opacity.
    this.style.opacity = '1';
    clearTimeout(this.timerId_);

    this.timerId_ = setTimeout(() => {
      this.style.opacity = '0';
      this.timerId_ = undefined;
    }, 2000);
  }

  protected getLabel_(): string {
    if (this.pageLabels) {
      return String(this.pageLabels[this.index]);
    }
    return String(this.index + 1);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-page-indicator': ViewerPageIndicatorElement;
  }
}

customElements.define(
    ViewerPageIndicatorElement.is, ViewerPageIndicatorElement);
