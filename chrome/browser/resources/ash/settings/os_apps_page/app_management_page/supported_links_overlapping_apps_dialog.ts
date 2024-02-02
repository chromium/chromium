// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {castExists} from 'chrome://resources/cr_components/app_management/util.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './supported_links_overlapping_apps_dialog.html.js';

export interface AppManagementSupportedLinksOverlappingAppsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const AppManagementSupportedLinksOverlappingAppsDialogElementBase =
    I18nMixin(PolymerElement);

export class AppManagementSupportedLinksOverlappingAppsDialogElement extends
    AppManagementSupportedLinksOverlappingAppsDialogElementBase {
  static get is() {
    return 'app-management-supported-links-overlapping-apps-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,

      apps: {
        type: Object,
      },

      overlappingAppIds: {
        type: Array,
      },
    };
  }

  app: App;
  overlappingAppIds: string[];
  apps: AppMap;

  private getBodyText_(apps: AppMap): string {
    const appNames: string[] = this.overlappingAppIds.map(appId => {
      return apps[appId]!.title!;
    });

    const appTitle = castExists(this.app.title);

    switch (appNames.length) {
      case 1:
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

  private onChangeClick_(): void {
    this.$.dialog.close();
  }

  private onCancelClick_(): void {
    this.$.dialog.cancel();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-supported-links-overlapping-apps-dialog':
        AppManagementSupportedLinksOverlappingAppsDialogElement;
  }
}

customElements.define(
    AppManagementSupportedLinksOverlappingAppsDialogElement.is,
    AppManagementSupportedLinksOverlappingAppsDialogElement);
