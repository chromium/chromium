// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that previews ambient settings in a small
 * container. Shows a cover image from a chosen album and text describing it.
 * Currently used on the ambient settings page.
 */

import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';
import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './ambient_zero_state_svg_element.js';
import '../../css/common.css.js';
import '../../css/cros_button_style.css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {AmbientUiVisibility} from '../../personalization_app.mojom-webui.js';
import {isAmbientModeAllowed, isScreenSaverPreviewEnabled} from '../load_time_booleans.js';

import {startScreenSaverPreview} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {AmbientPreviewBase} from './ambient_preview_base.js';
import {getTemplate} from './ambient_preview_small_element.html.js';

export interface AmbientPreviewSmall {
  $: {
    container: HTMLDivElement,
  };
}

export class AmbientPreviewSmall extends AmbientPreviewBase {
  static get is() {
    return 'ambient-preview-small';
  }

  static get template() {
    return getTemplate();
  }

  static override get properties() {
    return {
      screenSaverPreviewActive_: {
        type: Boolean,
        computed: 'computeScreenSaverPreviewActive_(ambientUiVisibility_)',
      },
      ambientUiVisibility_: {
        type: Number,
        value: null,
      },
      isScreenSaverPreviewEnabled_: {
        type: Boolean,
        value() {
          return isScreenSaverPreviewEnabled();
        },
      },
    };
  }

  private screenSaverPreviewActive_: boolean;
  private ambientUiVisibility_: AmbientUiVisibility|null;
  private isScreenSaverPreviewEnabled_: boolean;

  override connectedCallback() {
    assert(
        isAmbientModeAllowed(),
        'ambient-preview-small requires ambient mode allowed');
    super.connectedCallback();
    this.watch(
        'ambientUiVisibility_', state => state.ambient.ambientUiVisibility);
    this.updateFromStore();
  }

  private computeScreenSaverPreviewActive_(): boolean {
    return this.ambientUiVisibility_ === AmbientUiVisibility.kPreview;
  }

  private startScreenSaverPreview_(event: Event) {
    event.stopPropagation();
    startScreenSaverPreview(getAmbientProvider());
  }

  private getScreenSaverPreviewClass_(): string {
    return this.screenSaverPreviewActive_ ?
        'preview-button-disabled secondary' :
        'preview-button secondary';
  }

  private getScreenSaverPreviewText_(): string {
    return this.screenSaverPreviewActive_ ?
        this.i18n('screenSaverPreviewDownloading') :
        this.i18n('screenSaverPreviewButton');
  }

  private getScreenSaverPreviewAriaLabel_(): string {
    return this.screenSaverPreviewActive_ ?
        this.i18n('screenSaverPreviewDownloadingAriaLabel') :
        this.i18n('screenSaverPreviewButtonAriaLabel');
  }

  private getScreenSaverPreviewRole_(): string {
    return this.screenSaverPreviewActive_ ? 'none' : 'button';
  }
}

customElements.define(AmbientPreviewSmall.is, AmbientPreviewSmall);
