// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './content_setting_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './content_settings_icons.css.js';
import {getHtml} from './content_settings_icons.html.js';
import type {ContentSettingImageState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class ContentSettingsIconsElement extends CrLitElement {
  static get is() {
    return 'content-settings-icons';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contentSettingImageStates: {type: Array},
    };
  }

  accessor contentSettingImageStates: ContentSettingImageState[] = [];
}

declare global {
  interface HTMLElementTagNameMap {
    'content-settings-icons': ContentSettingsIconsElement;
  }
}

customElements.define(
    ContentSettingsIconsElement.is, ContentSettingsIconsElement);
