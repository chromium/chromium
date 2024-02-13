// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

import 'chrome://resources/ash/common/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './nearby_shared_icons.html.js';
import './nearby_device_icon.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './nearby_device.html.js';
import type {NearbyDeviceIconElement} from './nearby_device_icon.js';
import type {ShareTarget} from './nearby_share.mojom-webui.js';

export class NearbyDeviceElement extends PolymerElement {
  static get is() {
    return 'nearby-device' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Expected to start as null, then change to a valid object before this
       * component is shown.
       */
      shareTarget: {
        type: Object,
        value: null,
      },

      /**
       * Whether this share target is selected.
       */
      isSelected: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** Size of the target image/icon in pixels. */
      targetImageSize: {
        type: Number,
        readOnly: true,
        value: 26,
      },
    };
  }

  isSelected: boolean;
  shareTarget: ShareTarget|null;
  targetImageSize: number;

  override ready(): void {
    super.ready();

    this.updateStyles({'--target-image-size': this.targetImageSize + 'px'});
    this.listenToTargetImageLoad_();
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
    [NearbyDeviceElement.is]: NearbyDeviceElement;
  }
}

customElements.define(NearbyDeviceElement.is, NearbyDeviceElement);
