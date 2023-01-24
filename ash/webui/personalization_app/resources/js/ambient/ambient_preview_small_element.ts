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

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {AmbientUiVisibility} from '../personalization_app.mojom-webui.js';

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
    };
  }

  private screenSaverPreviewActive_: boolean;
  private ambientUiVisibility_: AmbientUiVisibility|null;

  override connectedCallback() {
    super.connectedCallback();
    this.watch(
        'ambientUiVisibility_', state => state.ambient.ambientUiVisibility);
    this.updateFromStore();
  }

  private computeScreenSaverPreviewActive_(): boolean {
    return this.ambientUiVisibility_ === AmbientUiVisibility.kPreview;
  }

  private isScreenSaverPreviewEnabled_() {
    return loadTimeData.getBoolean('isScreenSaverPreviewEnabled');
  }

  private startScreenSaverPreview_(event: Event) {
    event.stopPropagation();
    startScreenSaverPreview(getAmbientProvider());
  }

  private getScreenSaverPreviewClass_(): string {
    return this.screenSaverPreviewActive_ ? 'preview-button-disabled' :
                                            'preview-button';
  }

  private getScreenSaverPreviewText_(): string {
    return this.screenSaverPreviewActive_ ?
        this.i18n('screenSaverPreviewDownloading') :
        this.i18n('screenSaverPreviewButton');
  }
}

customElements.define(AmbientPreviewSmall.is, AmbientPreviewSmall);
