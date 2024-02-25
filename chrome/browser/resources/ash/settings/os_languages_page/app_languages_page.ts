// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {alphabeticalSort, getAppIcon} from 'chrome://resources/cr_components/app_management/util.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppLanguageSelectionDialogEntryPoint} from '../common/app_language_selection_dialog/app_language_selection_dialog.js';
import {AppManagementBrowserProxy} from '../common/app_management/browser_proxy.js';
import {AppMap} from '../common/app_management/store.js';
import {AppManagementStoreMixin} from '../common/app_management/store_mixin.js';
import {PrefsState} from '../common/types.js';

import {getTemplate} from './app_languages_page.html.js';

const DEVICE_LANGUAGE_LOCALE_TAG = '';

const OsSettingsAppsPageElementBase =
    AppManagementStoreMixin(I18nMixin(PolymerElement));

export interface OsSettingsAppLanguagesPageElement {
  $: {
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class OsSettingsAppLanguagesPageElement extends
    OsSettingsAppsPageElementBase {
  static get is() {
    return 'os-settings-app-languages-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
      appList_: {
        type: Array,
        value: () => [],
      },
      selectedApp_: Object,
      showSelectLanguageDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  // Internal state.
  private appList_: App[];
  private selectedApp_?: App;
  private showSelectLanguageDialog_: boolean;

  override connectedCallback(): void {
    super.connectedCallback();

    this.watch(
        'appList_',
        state => this.computeAppsSupportingPerAppLanguage_(state.apps));
    this.updateFromStore();
  }

  // UI i18n and icon helper methods.

  /**
   * App languages description based on `appList_` entry.
   */
  private getAppLanguagesDescription(): string {
    return this.appList_.length === 0 ?
        this.i18n('appLanguagesNoAppsSupportedDescription') :
        this.i18n('appLanguagesSupportedAppsDescription');
  }

  /**
   * Three-dots button accessibility label.
   */
  private getChangeLanguageButtonDescription_(app: App): string {
    assert(app.title);
    return this.i18n(
        'appLanguagesChangeLanguageButtonDescription', app.title,
        this.getSelectedLocale_(app));
  }

  /**
   * Returns display name of the selected locale if exists.
   */
  private getSelectedLocale_(app: App): string {
    if (app.selectedLocale?.localeTag) {
      const displayName = app.selectedLocale!.displayName;
      return displayName || app.selectedLocale!.localeTag;
    }
    return this.i18n('appLanguageDeviceLanguageLabel');
  }

  private iconUrlFromApp_(app: App): string {
    return getAppIcon(app);
  }

  // Stateful button click operations.
  // Since dropdown menu is not bound to any app entry, it doesn't know which
  // app it belongs to, hence we need to keep a state of the last chosen app
  // entry to the be passed whenever "Edit language" or "Reset language" is
  // chosen.

  /**
   * Shows dropdown menu to either edit language selection or reset to system
   * default.
   * Also saves the selected app entry.
   */
  private onDotsClick_(e: DomRepeatEvent<App>): void {
    // Sets a copy of the App object since it is not data-bound to
    // the `appList_` directly.
    this.selectedApp_ = e.model.item;

    const menu = this.$.menu.get();
    // Safety: This event comes from the DOM, so the target should always be an
    // element.
    menu.showAt(e.target as HTMLElement);
  }

  /**
   * Opens language selection dialog.
   */
  private onEditLanguageClick(): void {
    this.$.menu.get().close();
    // Safety: This method is only called from the action menu, which only
    // appears when `onDotsClick_()` is called, so `this.selectedApp_` should
    // always be defined here.
    assert(this.selectedApp_);
    this.showSelectLanguageDialog_ = true;
  }

  /**
   * Directly resets locale to system default.
   */
  private onResetLanguageClick(): void {
    this.$.menu.get().close();
    // Safety: This method is only called from the action menu, which only
    // appears when `onDotsClick_()` is called, so `this.selectedApp_` should
    // always be defined here.
    assert(this.selectedApp_);
    AppManagementBrowserProxy.getInstance().handler.setAppLocale(
        this.selectedApp_.id,
        DEVICE_LANGUAGE_LOCALE_TAG,
    );
  }

  private onSelectLanguageDialogClose_(): void {
    this.showSelectLanguageDialog_ = false;
    this.selectedApp_ = undefined;
  }

  // Process raw app list to a filtered app list.

  /**
   * Only show apps supporting per-app-language, and sort ascending.
   */
  private computeAppsSupportingPerAppLanguage_(apps: AppMap): App[] {
    const filteredApps: App[] = Object.values(apps).filter(app => {
      return app.supportedLocales.length > 0;
    });

    return filteredApps.sort((a, b) => {
      assert(a!.title);
      assert(b!.title);
      return alphabeticalSort(a.title, b.title);
    });
  }

  private getDialogEntryPoint_(): AppLanguageSelectionDialogEntryPoint {
    return AppLanguageSelectionDialogEntryPoint.LANGUAGES_PAGE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsAppLanguagesPageElement.is]: OsSettingsAppLanguagesPageElement;
  }
}

customElements.define(
    OsSettingsAppLanguagesPageElement.is, OsSettingsAppLanguagesPageElement);
