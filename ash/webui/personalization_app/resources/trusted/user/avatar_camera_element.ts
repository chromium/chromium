// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The avatar-camera component displays a camera interface to
 * allow the user to take a selfie.
 */

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


export class AvatarCamera extends PolymerElement {
  static get is() {
    return 'avatar-camera';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Set open property to turn the camera on/off and toggle the camera UI.
       */
      open: {
        type: Boolean,
        notify: true,
        observer: 'onOpenChanged_',
      },
    };
  }

  public open: boolean;

  // Hide/show the element html when open changes.
  private onOpenChanged_ = (open: boolean) => this.hidden = !open;
  private onCloseDialog_ = () => this.open = false;
}

customElements.define(AvatarCamera.is, AvatarCamera);
