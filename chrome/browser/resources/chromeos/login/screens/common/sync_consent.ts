// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
// <if expr="_google_chrome">
import '//oobe/sync-consent-icons.m.js';
// </if>

import '../../components/buttons/oobe_text_button.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/hd_iron_icon.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {CrCheckboxElement} from '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './sync_consent.html.js';


/**
 * UI mode for the dialog.
 */
enum SyncUiSteps {
  ASH_SYNC = 'ash-sync',
  LOADING = 'loading',
  LACROS_OVERVIEW = 'lacros-overview',
  LACROS_CUSTOMIZE = 'lacros-customize',
}


/**
 * Available user actions.
 */
enum UserAction {
  CONTINUE = 'continue',
  SYNC_EVERYTHING = 'sync-everything',
  SYNC_CUSTOM = 'sync-custom',
  LACROS_DECLINE = 'lacros-decline',
}


/**
 *  A set of flags of sync options for ChromeOS OOBE.
 */
interface OsSyncItems {
  osApps: boolean;
  osPreferences: boolean;
  osWifiConfigurations: boolean;
  osWallpaper: boolean;
}

const SyncConsentScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface SyncConsentScreenData {
  isLacrosEnabled: boolean;
}

export class SyncConsentScreen extends SyncConsentScreenElementBase {
  static get is() {
    return 'sync-consent-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * OS Sync options status.
       */
      osSyncItemsStatus: {
        type: Object,
        notify: true,
      },

      /**
       * Indicates whether user is minor mode user (e.g. under age of 18).
       */
      isMinorMode: Boolean,

      /**
       * Indicates whether Lacros is enabled.
       */
      isLacrosEnabled: Boolean,

      /**
       * The text key for the opt-in button (it could vary based on whether
       * the user is in minor mode).
       */
      optInButtonTextKey: {
        type: String,
        computed: 'getOptInButtonTextKey(isMinorMode)',
      },

      /**
       * Array of strings of the consent description elements
       */
      consentDescription: {
        type: Array,
      },

      /**
       * The text of the consent confirmation element.
       */
      consentConfirmation: {
        type: String,
      },


    };
  }

  osSyncItemsStatus: OsSyncItems;
  private isMinorMode: boolean;
  private isLacrosEnabled: boolean;
  private optInButtonTextKey: string;
  private consentDescription: string[];
  private consentConfirmation: string;


  constructor() {
    super();

    this.isMinorMode = false;
    this.isLacrosEnabled = false;
    this.osSyncItemsStatus = {
      osApps: true,
      osPreferences: true,
      osWifiConfigurations: true,
      osWallpaper: true,
    };
  }

  override get EXTERNAL_API(): string[] {
    return ['showLoadedStep', 'setIsMinorMode'];
  }

  override get UI_STEPS() {
    return SyncUiSteps;
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   */
  override onBeforeShow(data: SyncConsentScreenData): void {
    super.onBeforeShow(data);
    this.isLacrosEnabled = data['isLacrosEnabled'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): SyncUiSteps {
    return SyncUiSteps.LOADING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('SyncConsentScreen');
    this.i18nUpdateLocale();
  }


  /**
   * Wallpaper sync is a special case; its implementation relies upon
   * OS Settings to be synced. Thus, the wallpaper label and toggle are
   * only enabled when the Settings sync toggle is on.
   */
  private onSettingsSyncedChanged(): void {
    this.set(
        'osSyncItemsStatus.osWallpaper', this.osSyncItemsStatus.osPreferences);
  }

  /**
   * Reacts to changes in loadTimeData.
   */
  override updateLocalizedContent(): void {
    this.i18nUpdateLocale();
  }


  /**
   * This is called when SyncScreenBehavior becomes Shown.
   */
  showLoadedStep(isSyncLacros: boolean): void {
    if (isSyncLacros) {
      this.showLacrosOverview();
    } else {
      this.showAshSync();
    }
  }

  /**
   * This is called to set ash-sync step.
   */
  private showAshSync(): void {
    this.setUIStep(SyncUiSteps.ASH_SYNC);
  }

  /**
   * This is called to set lacros-overview step.
   */
  private showLacrosOverview(): void {
    this.setUIStep(SyncUiSteps.LACROS_OVERVIEW);
  }

  /**
   * This is called to set lacros-customize step.
   */
  private showLacrosCustomize(): void {
    this.setUIStep(SyncUiSteps.LACROS_CUSTOMIZE);
  }

  /**
   * Set the minor mode flag, which controls whether we could use nudge
   * techinuque on the UI.
   */
  setIsMinorMode(isMinorMode: boolean): void {
    this.isMinorMode = isMinorMode;
  }

  /**
   * Continue button is clicked
   */
  private onSettingsSaveAndContinue(e: Event, optedIn: boolean): void {
    assert(e.composedPath());
    let checkedBox: boolean = false;
    const reviewSettingsBox =
        this.shadowRoot?.querySelector<CrCheckboxElement>('#reviewSettingsBox');
    if (reviewSettingsBox instanceof CrCheckboxElement) {
      checkedBox = reviewSettingsBox.checked;
    }
    this.userActed([
      UserAction.CONTINUE,
      optedIn,
      checkedBox,
      this.getConsentDescription(),
      this.getConsentConfirmation(e.composedPath() as HTMLElement[]),
    ]);
  }

  private onAccepted(e: Event): void {
    this.onSettingsSaveAndContinue(e, true /* optedIn */);
  }

  private onDeclined(e: Event): void {
    this.onSettingsSaveAndContinue(e, false /* optedIn */);
  }

  /**
   * @param path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return The text of the consent confirmation element.
   */
  private getConsentConfirmation(path: HTMLElement[]): string {
    for (const element of path) {
      if (!element.hasAttribute) {
        continue;
      }

      if (element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }

      // Search down in case of click on a button with description below.
      const labels = element.querySelectorAll('[consent-confirmation]');
      if (labels && labels.length > 0) {
        assert(labels.length === 1);

        let result = '';
        for (const label of labels) {
          result += label.innerHTML.trim();
        }
        return result;
      }
    }
    assertNotReached('No consent confirmation element found.');
  }

  /** return Text of the consent description elements. */
  private getConsentDescription(): string[] {
    const consentDescriptionElements =
        this.shadowRoot?.querySelectorAll('[consent-description]');
    assert(consentDescriptionElements);
    const consentDescription =
        Array.from(consentDescriptionElements)
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  }

  private getReviewSettingText(locale: string, isArcRestricted: boolean):
      string {
    if (isArcRestricted) {
      return this.i18nDynamic(
          locale, 'syncConsentReviewSyncOptionsWithArcRestrictedText');
    }
    return this.i18nDynamic(locale, 'syncConsentReviewSyncOptionsText');
  }

  /**
   * @return The text key of the accept button.
   */
  private getOptInButtonTextKey(isMinorMode: boolean): string {
    return isMinorMode ? 'syncConsentTurnOnSync' :
                         'syncConsentAcceptAndContinue';
  }

  private onSyncEverything(e: Event): void {
    this.userActed([
      UserAction.SYNC_EVERYTHING,
      this.getConsentDescription(),
      this.getConsentConfirmation((e.composedPath()) as HTMLElement[]),
    ]);
  }

  private onManageClicked(e: Event): void {
    this.consentDescription = this.getConsentDescription();
    this.consentConfirmation =
        this.getConsentConfirmation((e.composedPath()) as HTMLElement[]);
    this.showLacrosCustomize();
  }

  private onBackClicked(): void {
    this.showLacrosOverview();
  }

  private onNextClicked(): void {
    this.userActed([
      UserAction.SYNC_CUSTOM,
      this.osSyncItemsStatus,
      this.consentDescription,
      this.consentConfirmation,
    ]);
  }

  private onLacrosDeclineClicked(): void {
    this.userActed(UserAction.LACROS_DECLINE);
  }

  private getAriaLabeltooltip(locale: string): string {
    return this.i18nDynamic(locale, 'syncConsentScreenOsSyncAppsTooltipText') +
        this.i18nDynamic(
            locale, 'syncConsentScreenOsSyncAppsTooltipAdditionalText');
  }

  private getAriaLabelToggleButtons(
      locale: string, title: string, subtitle: string): string {
    return this.i18nDynamic(locale, title) + '. ' +
        this.i18nDynamic(locale, subtitle);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [SyncConsentScreen.is]: SyncConsentScreen;
  }
}

customElements.define(SyncConsentScreen.is, SyncConsentScreen);
