// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import type {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import type {BackgroundImage} from './new_tab_page.mojom-webui.js';
import {strictQuery} from './utils.js';
import {WindowProxy} from './window_proxy.js';

/**
 * @fileoverview The background manager brokers access to background related
 * DOM elements. The reason for this abstraction is that the these elements are
 * not owned by any custom elements (this is done so that the aforementioned DOM
 * elements load faster at startup).
 *
 * The background manager expects an iframe with ID 'backgroundImage' to be
 * present in the DOM. It will use that element to set the background image URL.
 */

/**
 * Installs a listener for background image load times and manages a
 * |PromiseResolver| that resolves to the captured load time.
 */
class LoadTimeResolver {
  private resolver_: PromiseResolver<number> = new PromiseResolver();
  private eventTracker_: EventTracker = new EventTracker();

  constructor(url: string) {
    this.eventTracker_.add(window, 'message', ({data}: MessageEvent) => {
      if (data.frameType === 'background-image' &&
          data.messageType === 'loaded' && url === data.url) {
        this.resolve_(data.time);
      }
    });
  }

  get promise(): Promise<number> {
    return this.resolver_.promise;
  }

  reject() {
    this.resolver_.reject();
    this.eventTracker_.removeAll();
  }

  private resolve_(loadTime: number) {
    this.resolver_.resolve(loadTime);
    this.eventTracker_.removeAll();
  }
}

let instance: BackgroundManager|null = null;

export class BackgroundManager {
  static getInstance(): BackgroundManager {
    return instance || (instance = new BackgroundManager());
  }

  static setInstance(newInstance: BackgroundManager) {
    instance = newInstance;
  }

  private backgroundImage_: HTMLIFrameElement;
  private loadTimeResolver_: LoadTimeResolver|null = null;
  private url_: string;

  constructor() {
    this.backgroundImage_ =
        strictQuery(document.body, '#backgroundImage', HTMLIFrameElement);
    this.url_ = this.backgroundImage_.src;
  }

  /**
   * Sets whether the background image should be shown.
   * @param show True, if the background image should be shown.
   */
  setShowBackgroundImage(show: boolean) {
    document.body.toggleAttribute('show-background-image', show);
  }

  /** Sets the background color. */
  setBackgroundColor(color: SkColor) {
    document.body.style.backgroundColor = skColorToRgba(color);
  }

  /** Sets the background image. */
  setBackgroundImage(image: BackgroundImage) {
    const url =
        new URL('chrome-untrusted://new-tab-page/custom_background_image');
    url.searchParams.append('url', image.url.url);
    if (image.url2x) {
      url.searchParams.append('url2x', image.url2x.url);
    }
    if (image.size) {
      url.searchParams.append('size', image.size);
    }
    if (image.repeatX) {
      url.searchParams.append('repeatX', image.repeatX);
    }
    if (image.repeatY) {
      url.searchParams.append('repeatY', image.repeatY);
    }
    if (image.positionX) {
      url.searchParams.append('positionX', image.positionX);
    }
    if (image.positionY) {
      url.searchParams.append('positionY', image.positionY);
    }
    if (url.href === this.url_) {
      return;
    }
    if (this.loadTimeResolver_) {
      this.loadTimeResolver_.reject();
      this.loadTimeResolver_ = null;
    }
    // We use |contentWindow.location.replace| because reloading the iframe by
    // setting its |src| adds a history entry.
    this.backgroundImage_.contentWindow!.location.replace(url.href);
    // We track the URL separately because |contentWindow.location.replace| does
    // not update the iframe's src attribute.
    this.url_ = url.href;
  }

  /**
   * Returns promise that resolves with the background image load time.
   *
   * The background image iframe proactively sends the load time as soon as it
   * has loaded. However, this could be before we have installed the message
   * listener in LoadTimeResolver. Therefore, we request the background image
   * iframe to resend the load time in case it has already loaded. With that
   * setup we ensure that the load time is (re)sent _after_ both the NTP top
   * frame and the background image iframe have installed the required message
   * listeners.
   */
  getBackgroundImageLoadTime(): Promise<number> {
    if (!this.loadTimeResolver_) {
      this.loadTimeResolver_ = new LoadTimeResolver(this.backgroundImage_.src);
      WindowProxy.getInstance().postMessage(
          this.backgroundImage_, 'sendLoadTime',
          'chrome-untrusted://new-tab-page');
    }
    return this.loadTimeResolver_.promise;
  }
}
