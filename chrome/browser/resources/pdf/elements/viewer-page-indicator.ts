// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {isRTL} from 'chrome://resources/js/util_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Viewport} from '../viewport.js';

import {getTemplate} from './viewer-page-indicator.html.js';

export class ViewerPageIndicatorElement extends PolymerElement {
  static get is() {
    return 'viewer-page-indicator';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      label: {type: String, value: '1'},

      index: {type: Number, observer: 'indexChanged'},

      pageLabels: {type: Array, value: null, observer: 'pageLabelsChanged'},
    };
  }

  label: string;
  index: number;
  pageLabels: number[]|null;
  timerId?: number;
  private viewport_: Viewport|null = null;

  override ready() {
    super.ready();
    const callback = this.fadeIn_.bind(this);
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
    clearTimeout(this.timerId);

    this.timerId = setTimeout(() => {
      this.style.opacity = '0';
      this.timerId = undefined;
    }, 2000);
  }

  pageLabelsChanged() {
    this.indexChanged();
  }

  indexChanged() {
    if (this.pageLabels) {
      this.label = String(this.pageLabels[this.index]);
    } else {
      this.label = String(this.index + 1);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-page-indicator': ViewerPageIndicatorElement;
  }
}

customElements.define(
    ViewerPageIndicatorElement.is, ViewerPageIndicatorElement);
