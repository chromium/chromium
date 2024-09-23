// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.html.js';
import './supported_links_dialog.js';
import './supported_links_overlapping_apps_dialog.js';

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import type {AppMap} from 'chrome://resources/cr_components/app_management/constants.js';
import {AppManagementUserAction, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';
import {castExists, recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import type {LocalizedLinkElement} from 'chrome://resources/cr_components/localized_link/localized_link.js';
import type {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import type {CrRadioGroupElement} from 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './supported_links_item.css.js';
import {getHtml} from './supported_links_item.html.js';
import type {SupportedLinksOverlappingAppsDialogElement} from './supported_links_overlapping_apps_dialog.js';
import {createDummyApp} from './web_app_settings_utils.js';

type PreferenceType = 'preferred'|'browser';
const PREFERRED_APP_PREF = 'preferred' as const;

export interface SupportedLinksItemElement {
  $: {
    heading: LocalizedLinkElement,
    preferredRadioButton: CrRadioButtonElement,
    browserRadioButton: CrRadioButtonElement,
  };
}

const SupportedLinksItemElementBase = I18nMixinLit(CrLitElement);

export class SupportedLinksItemElement extends SupportedLinksItemElementBase {
  static get is() {
    return 'app-management-supported-links-item';
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

      hidden: {
        type: Boolean,
        reflect: true,
      },

      disabled_: {type: Boolean},

      showSupportedLinksDialog_: {type: Boolean},

      showOverlappingAppsDialog_: {type: Boolean},

      overlappingAppsWarning_: {type: String},

      showOverlappingAppsWarning_: {
        type: Boolean,
        reflect: true,
      },

      apps: {type: Object},

      overlappingAppIds_: {type: Array},
    };
  }

  app: App = createDummyApp();
  apps: AppMap = {};
  override hidden: boolean = false;
  protected disabled_: boolean = false;
  protected overlappingAppsWarning_: string = '';
  protected overlappingAppIds_: string[] = [];
  protected showOverlappingAppsDialog_: boolean = false;
  protected showOverlappingAppsWarning_: boolean = false;
  protected showSupportedLinksDialog_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('app')) {
      this.hidden = this.isHidden_();
      this.disabled_ = this.isDisabled_();
    }

    if (changedProperties.has('app') || changedProperties.has('apps')) {
      this.updateOverlappingAppsWarning_();
    }
  }

  /**
   * The supported links item is not available when an app has no supported
   * links.
   */
  private isHidden_(): boolean {
    return !this.app.supportedLinks.length;
  }

  /**
   * Disable the radio button options if the app is a PWA and is set to open
   * in the browser.
   */
  private isDisabled_(): boolean {
    return this.app.type === AppType.kWeb &&
        this.app.windowMode === WindowMode.kBrowser;
  }

  protected getCurrentPreferredApp_(): string {
    return this.app.isPreferredApp ? 'preferred' : 'browser';
  }

  protected getPreferredLabel_(): string {
    return this.i18n(
        'appManagementIntentSharingOpenAppLabel', String(this.app.title));
  }

  protected getDisabledExplanation_(): TrustedHTML {
    return this.i18nAdvanced(
        'appManagementIntentSharingTabExplanation',
        {substitutions: [String(this.app.title)]});
  }

  private async updateOverlappingAppsWarning_(): Promise<void> {
    if (this.app.isPreferredApp) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    let overlappingAppIds: string[] = [];
    try {
      const {appIds: appIds} =
          await BrowserProxy.getInstance().handler.getOverlappingPreferredApps(
              this.app.id);
      overlappingAppIds = appIds;
    } catch (err) {
      // If we fail to get the overlapping preferred apps, do not
      // show the overlap warning.
      console.warn(err);
      this.showOverlappingAppsWarning_ = false;
      return;
    }
    this.overlappingAppIds_ = overlappingAppIds;

    const appNames = overlappingAppIds.map(appId => this.apps[appId]!.title!);
    if (appNames.length === 0) {
      this.showOverlappingAppsWarning_ = false;
      return;
    }

    switch (appNames.length) {
      case 1:
        assert(appNames[0]);
        this.overlappingAppsWarning_ = this.i18n(
            'appManagementIntentOverlapWarningText1App', appNames[0]);
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

  protected launchDialog_(e: CustomEvent<{event: Event}>): void {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.showSupportedLinksDialog_ = true;

    recordAppManagementUserAction(
        this.app.type, AppManagementUserAction.SUPPORTED_LINKS_LIST_SHOWN);
  }

  protected onDialogClose_(): void {
    this.showSupportedLinksDialog_ = false;
    focusWithoutInk(this.$.heading);
  }

  /* Preferred app state change dialog and related functions ******************/

  protected async onSupportedLinkPrefChanged_(
      event: CustomEvent<{value: string}>): Promise<void> {
    const preference = event.detail.value as PreferenceType;
    const previous = this.getCurrentPreferredApp_() as PreferenceType;
    if (preference === previous) {
      return;
    }

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

  protected onOverlappingDialogClosed_(): void {
    this.showOverlappingAppsDialog_ = false;

    const overlapDialog = castExists(
        this.shadowRoot!
            .querySelector<SupportedLinksOverlappingAppsDialogElement>(
                '#overlapDialog'));
    if (overlapDialog.wasConfirmed()) {
      this.setAppAsPreferredApp_(PREFERRED_APP_PREF);
      // Return keyboard focus to the preferred radio button.
      focusWithoutInk(this.$.preferredRadioButton);
    } else {
      // Reset the radio button.
      this.shadowRoot!.querySelector<CrRadioGroupElement>(
                          '#radioGroup')!.selected =
          this.getCurrentPreferredApp_();
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
    'app-management-supported-links-item': SupportedLinksItemElement;
  }
}

customElements.define(SupportedLinksItemElement.is, SupportedLinksItemElement);
