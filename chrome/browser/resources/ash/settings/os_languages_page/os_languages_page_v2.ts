// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page-v2' is the languages sub-page
 * for languages and inputs settings.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/js/action_link.js';
import 'chrome://resources/ash/common/cr_elements/action_link.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import './change_device_language_dialog.js';
import './os_add_languages_dialog.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../controls/settings_toggle_button.js';
import '../settings_shared.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrActionMenuElement} from 'chrome://resources/ash/common/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrLazyRenderElement} from 'chrome://resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './languages_metrics_proxy.js';
import {LanguageHelper, LanguagesModel, LanguageState} from './languages_types.js';
import {getTemplate} from './os_languages_page_v2.html.js';

/**
 * Millisecond delay that can be used when closing an action menu to keep it
 * briefly on-screen so users can see the changes.
 */
const MENU_CLOSE_DELAY = 100;

const OsSettingsLanguagesPageV2ElementBase =
    RouteObserverMixin(PrefsMixin(I18nMixin(DeepLinkingMixin(PolymerElement))));

export interface OsSettingsLanguagesPageV2Element {
  $: {
    addLanguages: CrButtonElement,
    menu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export class OsSettingsLanguagesPageV2Element extends
    OsSettingsLanguagesPageV2ElementBase {
  static get is() {
    return 'os-settings-languages-page-v2' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Read-only reference to the languages model provided by the
       * 'os-settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,

      /**
       * The language to display the details for and its index.
       */
      detailLanguage_: Object,

      showAddLanguagesDialog_: Boolean,

      showChangeDeviceLanguageDialog_: {
        type: Boolean,
        value: false,
      },

      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isGuest');
        },
      },

      isSecondaryUser_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isSecondaryUser');
        },
      },

      primaryUserEmail_: {
        type: String,
        value() {
          return loadTimeData.getString('primaryUserEmail');
        },
      },

      isPerAppLanguageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isPerAppLanguageEnabled');
        },
      },

      languageSettingsV2Update2Enabled_: Boolean,
    };
  }

  // Public API: Bidirectional data flow.
  // override prefs: any;  // From PrefsMixin.

  // Public API: Downwards data flow.
  languages: LanguagesModel|undefined;
  languageHelper: LanguageHelper;

  // API proxies.
  private languagesMetricsProxy_ = LanguagesMetricsProxyImpl.getInstance();

  // Internal properties for mixins.
  // From DeepLinkingMixin.
  override supportedSettingIds = new Set([
    Setting.kAddLanguage,
    Setting.kChangeDeviceLanguage,
    Setting.kOfferTranslation,
  ]);

  // Internal state.
  private detailLanguage_?: {state: LanguageState, index: number};
  // This property does not have a default value in `static get properties()`.
  // TODO(b/265556480): Update the initial value to be false.
  private showAddLanguagesDialog_: boolean;
  private showChangeDeviceLanguageDialog_: boolean;

  // loadTimeData flags and strings.
  private isGuest_: boolean;
  private isSecondaryUser_: boolean;
  private primaryUserEmail_: string;
  private isPerAppLanguageEnabled_: boolean;
  // TODO: b/263823772 - Inline this variable.
  private languageSettingsV2Update2Enabled_ = true;

  override currentRouteChanged(route: Route): void {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_LANGUAGES) {
      return;
    }

    this.attemptDeepLink();
  }

  private getLanguageDisplayName_(language: string): string {
    // This `getLanguage` assertion is potentially unsafe and could fail.
    // TODO(b/265554088): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    return this.languageHelper.getLanguage(language)!.displayName;
  }

  private onChangeDeviceLanguageClick_(): void {
    this.showChangeDeviceLanguageDialog_ = true;
  }

  private onChangeDeviceLanguageDialogClose_(): void {
    this.showChangeDeviceLanguageDialog_ = false;
    focusWithoutInk(
        // Safety: This method is only called when the change device
        // language dialog is closed, but that can only be opened if
        // #changeDeviceLanguage was clicked.
        castExists(this.shadowRoot!.querySelector('#changeDeviceLanguage')));
  }

  private getChangeDeviceLanguageButtonDescription_(language: string): string {
    return this.i18n(
        'changeDeviceLanguageButtonDescription',
        this.getLanguageDisplayName_(language));
  }

  /**
   * Navigates to app languages subpage.
   */
  private onAppLanguagesClick_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_APP_LANGUAGES);
  }

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   */
  private onAddLanguagesClick_(e: Event): void {
    e.preventDefault();
    this.languagesMetricsProxy_.recordAddLanguages();
    this.showAddLanguagesDialog_ = true;
  }

  private onAddLanguagesDialogClose_(): void {
    this.showAddLanguagesDialog_ = false;
    focusWithoutInk(this.$.addLanguages);
  }

  /**
   * Checks if there are supported languages that are not enabled but can be
   * enabled.
   * @return True if there is at least one available language.
   */
  private canEnableSomeSupportedLanguage_(languages: LanguagesModel|
                                          undefined): boolean {
    return languages !== undefined && languages.supported.some(language => {
      return this.languageHelper.canEnableLanguage(language);
    });
  }

  /**
   * @return True if the translate checkbox should be disabled.
   */
  private disableTranslateCheckbox_(): boolean {
    if (!this.detailLanguage_ || !this.detailLanguage_.state) {
      return true;
    }

    const languageState = this.detailLanguage_.state;
    if (!languageState.language || !languageState.language.supportsTranslate) {
      return true;
    }

    if (this.languageHelper.isOnlyTranslateBlockedLanguage(languageState)) {
      return true;
    }

    // This assertion of `this.languages` is potentially unsafe and could fail.
    // TODO(b/265553377): Prove that this assertion is safe, or rewrite this to
    // avoid this assertion.
    return this.languageHelper.convertLanguageCodeForTranslate(
               languageState.language.code) === this.languages!.translateTarget;
  }

  /**
   * Handler for changes to the translate checkbox.
   */
  private onTranslateCheckboxChange_(e: CustomEvent<boolean>): void {
    // Safety: This method is only called from a 'change' event from a
    // <cr-checkbox>, so the event target must be a <cr-checkbox>.
    const checked = (e.target as CrCheckboxElement).checked;
    if (checked) {
      this.languageHelper.enableTranslateLanguage(
          // Safety: This method is only called from the action menu, which only
          // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
          // should always be defined here.
          this.detailLanguage_!.state.language.code);
    } else {
      this.languageHelper.disableTranslateLanguage(
          // Safety: This method is only called from the action menu, which only
          // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
          // should always be defined here.
          this.detailLanguage_!.state.language.code);
    }
    this.languagesMetricsProxy_.recordTranslateCheckboxChanged(checked);
    recordSettingChange(
        checked ? Setting.kEnableTranslateLanguage :
                  Setting.kDisableTranslateLanguage);
    this.closeMenuSoon_();
  }

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * clicked it can be seen to change state before disappearing.
   */
  private closeMenuSoon_(): void {
    const menu = this.$.menu.get();
    setTimeout(() => {
      if (menu.open) {
        menu.close();
      }
    }, MENU_CLOSE_DELAY);
  }

  /**
   * @return True if the "Move to top" option for |language| should be visible.
   */
  private showMoveToTop_(): boolean {
    // "Move To Top" is a no-op for the top language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index === 0;
  }

  /**
   * @return True if the "Move up" option for |language| should be visible.
   */
  private showMoveUp_(): boolean {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== 0 && this.detailLanguage_.index !== 1;
  }

  /**
   * @return True if the "Move down" option for |language| should be visible.
   */
  private showMoveDown_(): boolean {
    return this.languages !== undefined && this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== this.languages.enabled.length - 1;
  }

  /**
   * Moves the language to the top of the list.
   */
  private onMoveToTopClick_(): void {
    this.$.menu.get().close();
    this.languageHelper.moveLanguageToFront(
        // Safety: This method is only called from the action menu, which only
        // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
        // should always be defined here.
        this.detailLanguage_!.state.language.code);
    recordSettingChange(Setting.kMoveLanguageToFront);
  }

  /**
   * Moves the language up in the list.
   */
  private onMoveUpClick_(): void {
    this.$.menu.get().close();
    this.languageHelper.moveLanguage(
        // Safety: This method is only called from the action menu, which only
        // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
        // should always be defined here.
        this.detailLanguage_!.state.language.code,
        /*upDirection=*/ true);
    recordSettingChange(Setting.kMoveLanguageUp);
  }

  /**
   * Moves the language down in the list.
   */
  private onMoveDownClick_(): void {
    this.$.menu.get().close();
    this.languageHelper.moveLanguage(
        // Safety: This method is only called from the action menu, which only
        // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
        // should always be defined here.
        this.detailLanguage_!.state.language.code,
        /*upDirection=*/ false);
    recordSettingChange(Setting.kMoveLanguageDown);
  }

  /**
   * Disables the language.
   */
  private onRemoveLanguageClick_(): void {
    this.$.menu.get().close();
    this.languageHelper.disableLanguage(
        // Safety: This method is only called from the action menu, which only
        // appears when `onDotsClick_()` is called, so `this.detailLanguage_`
        // should always be defined here.
        this.detailLanguage_!.state.language.code);
    recordSettingChange(Setting.kRemoveLanguage);
  }

  private onDotsClick_(e: DomRepeatEvent<LanguageState>): void {
    // Sets a copy of the LanguageState object since it is not data-bound to
    // the languages model directly.
    this.detailLanguage_ = {
      state: e.model.item,
      index: e.model.index,
    };

    const menu = this.$.menu.get();
    // Safety: This event comes from the DOM, so the target should always be an
    // element.
    menu.showAt(e.target as HTMLElement);
  }

  private onTranslateToggleChange_(e: CustomEvent<unknown>): void {
    this.languagesMetricsProxy_.recordToggleTranslate(
        // Safety: This method is only called from a
        // 'settings-boolean-control-changed' event from a
        // <settings-toggle-button>, so the event target must be a
        // <settings-toggle-button>.
        (e.target as SettingsToggleButtonElement).checked);
  }

  /**
   * @param languageCode The language code identifying a language.
   * @param translateTarget The translate target language.
   * @return class name for whether it's a translate-target or not.
   */
  private getTranslationTargetClass_(
      languageCode: string, translateTarget: string): string {
    return this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
            translateTarget ?
        'translate-target' :
        'non-translate-target';
  }

  private getOfferTranslationLabel_(update2Enabled: boolean): string {
    return this.i18n(
        update2Enabled ? 'offerGoogleTranslateLabel' : 'offerTranslationLabel');
  }

  private getOfferTranslationSublabel_(update2Enabled: boolean): string {
    return update2Enabled ? '' : this.i18n('offerTranslationSublabel');
  }

  private getLanguagePreferenceTitle_(update2Enabled: boolean): string {
    return this.i18n(
        update2Enabled ? 'websiteLanguagesTitle' : 'languagesPreferenceTitle');
  }

  private getLanguagePreferenceDescription_(update2Enabled: boolean):
      TrustedHTML {
    return this.i18nAdvanced(
        update2Enabled ? 'websiteLanguagesDescription' :
                         'languagesPreferenceDescription');
  }

  private openManageGoogleAccountLanguage_(): void {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE);
    window.open(loadTimeData.getString('googleAccountLanguagesURL'));
  }

  private onLanguagePreferenceDescriptionLinkClick_(): void {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_WEB_LANGUAGES_LEARN_MORE);
  }
}

customElements.define(
    OsSettingsLanguagesPageV2Element.is, OsSettingsLanguagesPageV2Element);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsLanguagesPageV2Element.is]: OsSettingsLanguagesPageV2Element;
  }
}
