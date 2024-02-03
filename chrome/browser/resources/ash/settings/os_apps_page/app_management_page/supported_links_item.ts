// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_management_cros_shared_style.css.js';
import './supported_links_dialog.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import './supported_links_dialog.js';
import './supported_links_overlapping_apps_dialog.js';

import {App, AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {AppManagementUserAction, AppMap, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';
import {castExists, recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {LocalizedLinkElement} from 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import {CrRadioButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import {CrRadioGroupElement} from 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './supported_links_item.html.js';
import {AppManagementSupportedLinksOverlappingAppsDialogElement} from './supported_links_overlapping_apps_dialog.js';

type PreferenceType = 'preferred'|'browser';
const PREFERRED_APP_PREF = 'preferred' as const;

export interface AppManagementSupportedLinksItemElement {
  $: {
    heading: LocalizedLinkElement,
    preferredRadioButton: CrRadioButtonElement,
    browserRadioButton: CrRadioButtonElement,
  };
}

const AppManagementSupportedLinksItemElementBase = I18nMixin(PolymerElement);

export class AppManagementSupportedLinksItemElement extends
    AppManagementSupportedLinksItemElementBase {
  static get is() {
    return 'app-management-supported-links-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,

      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },

      disabled_: {
        type: Boolean,
        computed: 'isDisabled_(app)',
      },

      showSupportedLinksDialog_: {
        type: Boolean,
        value: false,
      },

      showOverlappingAppsDialog_: {
        type: Boolean,
        value: false,
      },

      overlappingAppsWarning_: {
        type: String,
      },

      showOverlappingAppsWarning_:
          {type: Boolean, value: false, reflectToAttribute: true},

      apps: Object,

      overlappingAppIds_: {
        type: Array,
      },
    };
  }

  static get observers() {
    return [
      'updateOverlappingAppsWarning_(apps, app)',
    ];
  }

  app: App;
  apps: AppMap;
  override hidden: boolean;
  private disabled_: boolean;
  private overlappingAppsWarning_: string;
  private overlappingAppIds_: string[];
  private showOverlappingAppsDialog_: boolean;
  private showOverlappingAppsWarning_: boolean;
  private showSupportedLinksDialog_: boolean;

  /**
   * The supported links item is not available when an app has no supported
   * links.
   */
  private isHidden_(app: App): boolean {
    return !app.supportedLinks.length;
  }

  /**
   * Disable the radio button options if the app is a PWA and is set to open
   * in the browser.
   */
  private isDisabled_(app: App): boolean {
    return app.type === AppType.kWeb && app.windowMode === WindowMode.kBrowser;
  }

  private getCurrentPreferredApp_(app: App): string {
    return app.isPreferredApp ? 'preferred' : 'browser';
  }

  private getPreferredLabel_(app: App): string {
    return this.i18n(
        'appManagementIntentSharingOpenAppLabel', String(app.title));
  }

  private getDisabledExplanation_(app: App): TrustedHTML {
    return this.i18nAdvanced(
        'appManagementIntentSharingTabExplanation',
        {substitutions: [String(app.title)]});
  }

  private async updateOverlappingAppsWarning_(
      apps: AppMap|undefined, app: App|undefined): Promise<void> {
    if (!apps || !app || app.isPreferredApp) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    let overlappingAppIds: string[] = [];
    try {
      const {appIds: appIds} =
          await BrowserProxy.getInstance().handler.getOverlappingPreferredApps(
              app.id);
      overlappingAppIds = appIds;
    } catch (err) {
      // If we fail to get the overlapping preferred apps, do not
      // show the overlap warning.
      console.warn(err);
      this.showOverlappingAppsWarning_ = false;
      return;
    }
    this.overlappingAppIds_ = overlappingAppIds;

    const appNames = overlappingAppIds.map(appId => apps[appId]!.title!);
    if (appNames.length === 0) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    switch (appNames.length) {
      case 1:
        this.overlappingAppsWarning_ =
            this.i18n('appManagementIntentOverlapWarningText1App', appNames[0]);
        break;
      case 2:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText2Apps', ...appNames);
        break;
      case 3:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText3Apps', ...appNames);
        break;
      case 4:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText4Apps',
            ...appNames.slice(0, 3));
        break;
      default:
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText5OrMoreApps',
            ...appNames.slice(0, 3), appNames.length - 3);
        break;
    }

    this.showOverlappingAppsWarning_ = true;
  }

  /* Supported links list dialog functions ************************************/

  private launchDialog_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.showSupportedLinksDialog_ = true;

    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.SUPPORTED_LINKS_LIST_SHOWN);
  }

  private onDialogClose_(): void {
    this.showSupportedLinksDialog_ = false;
    focusWithoutInk(this.$.heading);
  }

  /* Preferred app state change dialog and related functions ******************/

  private async onSupportedLinkPrefChanged_(
      event: CustomEvent<{value: string}>): Promise<void> {
    const preference = event.detail.value as PreferenceType;

    let overlappingAppIds: string[] = [];
    try {
      const {appIds: appIds} =
          await BrowserProxy.getInstance().handler.getOverlappingPreferredApps(
              this.app.id);
      overlappingAppIds = appIds;
    } catch (err) {
      // If we fail to get the overlapping preferred apps, don't prevent the
      // user from setting their preference.
      console.warn(err);
    }

    // If there are overlapping apps, show the overlap dialog to the user.
    if (preference === PREFERRED_APP_PREF && overlappingAppIds.length > 0) {
      this.overlappingAppIds_ = overlappingAppIds;
      this.showOverlappingAppsDialog_ = true;
      recordAppManagementUserAction(
          this.app.type, AppManagementUserAction.OVERLAPPING_APPS_DIALOG_SHOWN);
      return;
    }

    this.setAppAsPreferredApp_(preference);
  }

  private onOverlappingDialogClosed_(): void {
    this.showOverlappingAppsDialog_ = false;

    const overlapDialog =
        castExists(this.shadowRoot!.querySelector<
                   AppManagementSupportedLinksOverlappingAppsDialogElement>(
            '#overlapDialog'));
    if (overlapDialog.wasConfirmed()) {
      this.setAppAsPreferredApp_(PREFERRED_APP_PREF);
      // Return keyboard focus to the preferred radio button.
      focusWithoutInk(this.$.preferredRadioButton);
    } else {
      // Reset the radio button.
      this.shadowRoot!.querySelector<CrRadioGroupElement>(
                          '#radioGroup')!.selected =
          this.getCurrentPreferredApp_(this.app);
      // Return keyboard focus to the browser radio button.
      focusWithoutInk(this.$.browserRadioButton);
    }
  }

  /**
   * Sets this.app as a preferred app or not depending on the value of
   * |preference|.
   */
  private setAppAsPreferredApp_(preference: PreferenceType): void {
    const newState = preference === PREFERRED_APP_PREF;

    BrowserProxy.getInstance().handler.setPreferredApp(this.app.id, newState);

    const userAction = newState ?
        AppManagementUserAction.PREFERRED_APP_TURNED_ON :
        AppManagementUserAction.PREFERRED_APP_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, userAction);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-supported-links-item':
        AppManagementSupportedLinksItemElement;
  }
}

customElements.define(
    AppManagementSupportedLinksItemElement.is,
    AppManagementSupportedLinksItemElement);
