// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-progress' component shows a progress indicator for
 * a Nearby Share transfer to a remote device. It shows device icon and name,
 * and a circular progress bar that can show either progress as a percentage or
 * an animation if the percentage is indeterminate.
 */

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import './nearby_shared_icons.html.js';
import './nearby_device_icon.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {NearbyDeviceIconElement} from './nearby_device_icon.js';
import {getTemplate} from './nearby_progress.html.js';
import type {ShareTarget} from './nearby_share.mojom-webui.js';

export class NearbyProgressElement extends PolymerElement {
  static get is() {
    return 'nearby-progress' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The share target to show the progress for. Expected to start as null,
       * then change to a valid object before this component is shown.
       */
      shareTarget: {
        type: Object,
        value: null,
      },

      /**
       * If true, displays an animation representing an unknown amount of
       * progress; otherwise, the progress bar is hidden.
       */
      showIndeterminateProgress: {
        type: Boolean,
        value: false,
      },

      /**
       * If true, then set progress stroke to red, stop any animation, show
       * 100% instead, and set icons to grey. If |showProgress| is |NONE|, then
       * the progress bar is still hidden.
       */
      hasError: {
        type: Boolean,
        value: false,
      },

      /** Size of the target image/icon in pixels. */
      targetImageSize: {
        type: Number,
        readOnly: true,
        value: 68,
      },
    };
  }

  hasError: boolean;
  shareTarget: ShareTarget|null;
  showIndeterminateProgress: boolean;
  targetImageSize: number;

  override ready(): void {
    super.ready();

    this.updateStyles({'--target-image-size': this.targetImageSize + 'px'});
    this.listenToTargetImageLoad_();
  }

  private getProgressWheelClass_(): string {
    const classes: string[] = [];
    if (this.hasError) {
      classes.push('has-error');
    }
    if (this.showIndeterminateProgress) {
      classes.push('indeterminate-progress');
    } else {
      classes.push('hidden');
    }
    return classes.join(' ');
  }

  /**
   * Allow focusing on the progress bar. Ignored by Chromevox otherwise.
   * The tabindex to be applied to the progress wheel.
   */
  private getProgressBarTabIndex_(): number {
    if (this.showIndeterminateProgress && !this.hasError) {
      return 0;
    }
    return -1;
  }

  private getTargetImageUrl_(): string {
    if (!(this.shareTarget && this.shareTarget.imageUrl &&
          this.shareTarget.imageUrl.url &&
          this.shareTarget.imageUrl.url.length)) {
      return '';
    }

    // Adds the parameter to resize to the desired size.
    return this.shareTarget.imageUrl.url + '=s' + this.targetImageSize;
  }

  private listenToTargetImageLoad_(): void {
    const autoImg =
        this.shadowRoot!.querySelector<HTMLImageElement>('#share-target-image');
    assert(autoImg);
    if (autoImg.complete && autoImg.naturalHeight !== 0) {
      this.onTargetImageLoad_();
    } else {
      autoImg.onload = () => {
        this.onTargetImageLoad_();
      };
    }
  }

  private onTargetImageLoad_(): void {
    this.shadowRoot!.querySelector<HTMLImageElement>(
                        '#share-target-image')!.style.display = 'inline';
    this.shadowRoot!.querySelector<NearbyDeviceIconElement>(
                        '#icon')!.style.display = 'none';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NearbyProgressElement.is]: NearbyProgressElement;
  }
}

customElements.define(NearbyProgressElement.is, NearbyProgressElement);
