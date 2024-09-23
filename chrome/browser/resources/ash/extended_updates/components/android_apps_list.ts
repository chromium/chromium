// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '../strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import type {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {App} from '../extended_updates.mojom-webui.js';

import {getTemplate} from './android_apps_list.html.js';

const AndroidAppsListElementBase = I18nMixin(PolymerElement);

export class AndroidAppsListElement extends AndroidAppsListElementBase {
  static get is() {
    return 'android-apps-list' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      apps: {
        type: Array<App>,
        value: [],
      },
    };
  }

  private apps: App[];

  private iconUrlFromId(app: App): string {
    return `chrome://app-icon/${app.id}/64`;
  }

  private hasApps(apps: App[]): boolean {
    return apps.length > 0;
  }

  private getDescription(): string {
    if (this.apps.length === 1) {
      return this.i18n('androidAppsListDescriptionSingular');
    }
    return this.i18n('androidAppsListDescriptionPlural', this.apps.length);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AndroidAppsListElement.is]: AndroidAppsListElement;
  }
}

customElements.define(AndroidAppsListElement.is, AndroidAppsListElement);
