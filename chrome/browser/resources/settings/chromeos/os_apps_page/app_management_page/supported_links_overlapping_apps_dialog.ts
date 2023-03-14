// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../../settings_shared.css.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../../assert_extras.js';

import {AppMap} from './store.js';
import {AppManagementStoreMixin} from './store_mixin.js';
import {getTemplate} from './supported_links_overlapping_apps_dialog.html.js';

export interface AppManagementSupportedLinksOverlappingAppsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const AppManagementSupportedLinksOverlappingAppsDialogElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

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

      apps_: {
        type: Object,
      },

      overlappingAppIds: {
        type: Array,
      },
    };
  }

  app: App;
  overlappingAppIds: string[];
  private apps_: AppMap;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  }

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
