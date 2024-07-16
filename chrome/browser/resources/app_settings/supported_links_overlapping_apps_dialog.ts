// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {castExists} from 'chrome://resources/cr_components/app_management/util.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app_management_shared_style.css.js';
import {getHtml} from './supported_links_overlapping_apps_dialog.html.js';
import {createDummyApp} from './web_app_settings_utils.js';

export interface SupportedLinksOverlappingAppsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SupportedLinksOverlappingAppsDialogElementBase =
    I18nMixinLit(CrLitElement);

export class SupportedLinksOverlappingAppsDialogElement extends
    SupportedLinksOverlappingAppsDialogElementBase {
  static get is() {
    return 'app-management-supported-links-overlapping-apps-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      app: {type: Object},
      apps: {type: Object},
      overlappingAppIds: {type: Array},
    };
  }

  app: App = createDummyApp();
  overlappingAppIds: string[] = [];
  apps: AppMap = {};

  protected getBodyText_(): string {
    const appNames: string[] = this.overlappingAppIds.map(appId => {
      return this.apps[appId]!.title!;
    });

    const appTitle = castExists(this.app.title);

    switch (appNames.length) {
      case 1:
        assert(appNames[0]);
        return this.i18n(
            'appManagementIntentOverlapDialogText1App', appTitle, appNames[0]);
      case 2:
        return this.i18n(
            'appManagementIntentOverlapDialogText2Apps', appTitle, ...appNames);
      case 3:
        return this.i18n(
            'appManagementIntentOverlapDialogText3Apps', appTitle, ...appNames);
      case 4:
        return this.i18n(
            'appManagementIntentOverlapDialogText4Apps', appTitle,
            ...appNames.slice(0, 3));
      default:
        return this.i18n(
            'appManagementIntentOverlapDialogText5OrMoreApps', appTitle,
            ...appNames.slice(0, 3), appNames.length - 3);
    }
  }

  wasConfirmed(): boolean {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  protected onChangeClick_(): void {
    this.$.dialog.close();
  }

  protected onCancelClick_(): void {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-supported-links-overlapping-apps-dialog':
        SupportedLinksOverlappingAppsDialogElement;
  }
}

customElements.define(
    SupportedLinksOverlappingAppsDialogElement.is,
    SupportedLinksOverlappingAppsDialogElement);
