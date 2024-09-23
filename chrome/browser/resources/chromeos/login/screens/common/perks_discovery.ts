// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for perks discovery screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './perks_discovery.html.js';

export const PerksDiscoveryElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

const GENERATE_WEB_VIEW_CSS = () => {
  return {
    code: `svg {
          width: 100%;
          height: 100%;
          background-color: ` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-app_base_shaded') +
        `;
          --cros-sys-illo-color1 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color1') +
        `;
          --cros-sys-illo-color1-1 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color1-1') +
        `;
          --cros-sys-illo-color1-2 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color1-2') +
        `;
          --cros-sys-illo-color2 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color2') +
        `;
          --cros-sys-illo-color3 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color3') +
        `;
          --cros-sys-illo-color4 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color4') +
        `;
          --cros-sys-illo-color5 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color5') +
        `;
          --cros-sys-illo-color6 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-color6') +
        `;
          --cros-sys-illo-base :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-base') +
        `;
          --cros-sys-illo-secondary :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-secondary') +
        `;
          --cros-sys-illo-on_primary_container :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-on_primary_container') +
        `;
          --cros-sys-illo-card-color1:` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-color1') +
        `;
          --cros-sys-illo-card-color2 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-color2') +
        `;
          --cros-sys-illo-card-color3 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-color3') +
        `;
          --cros-sys-illo-card-color4 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-color4') +
        `;
          --cros-sys-illo-card-color5 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-color5') +
        `;
          --cros-sys-illo-card-on_color1 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-on_color1') +
        `;
          --cros-sys-illo-card-on_color2 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-on_color2') +
        `;
          --cros-sys-illo-card-on_color3 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-on_color3') +
        `;
          --cros-sys-illo-card-on_color4 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-on_color4') +
        `;
          --cros-sys-illo-card-on_color5 :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-card-on_color5') +
        `;
          --cros-sys-app_base_shaded :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-app_base_shaded') +
        `;
          --cros-sys-app_base :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-app_base') +
        `;
          --cros-sys-base_elevated :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-base_elevated') +
        `;
          --cros-sys-illo-analog :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-analog') +
        `;
          --cros-sys-illo-muted :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-muted') +
        `;
          --cros-sys-illo-complement :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-complement') +
        `;
          --cros-sys-illo-on_gradient :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-on_gradient') +
        `;
          --cros-sys-illo-analog_variant :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-analog_variant') +
        `;
          --cros-sys-illo-muted_variant :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-muted_variant') +
        `;
          --cros-sys-illo-complement_variant :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-complement_variant') +
        `;
          --cros-sys-illo-on_gradient_variant :` +
        getComputedStyle(document.body)
            .getPropertyValue('--cros-sys-illo-on_gradient_variant') +
        `;
          --cros-sys-primary: ` +
        getComputedStyle(document.body).getPropertyValue('--cros-sys-primary') +
        `;
    }`,
  };
};

/**
 * Available user actions.
 */
enum UserAction {
  FINISHED = 'finished',
  LOADED = 'loaded',
}

interface PerkData {
  perkId: string;
  title: string;
  subtitle: string;
  iconUrl: string;
  additionalText: string;
  illustrationUrl: string;
  illustrationWidth: string;
  illustrationHeight: string;
  primaryButtonLabel: string;
  secondaryButtonLabel: string;
}

enum PerksDiscoveryStep {
  LOADING = 'loading',
  OVERVIEW = 'overview',
}


export class PerksDiscoveryElement extends PerksDiscoveryElementBase {
  static get is() {
    return 'perks-discovery-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * List of perks to display.
       */
      perksList: {
        type: Array,
        value: [],
        notify: true,
      },
      /**
       * A map that stores a user's interest in various perks.
       */
      selectedPerks: {
        type: Object,
      },
      /**
       * Current perk displayed.
       */
      currentPerk: {
        type: Number,
        value: -1,
      },
      /**
       * Number of dom repeat icons rendered items.
       */
      itemIconsRendered: {
        type: Number,
        value: 0,
      },
      /**
       * Number of dom repeat illustration rendered items.
       */
      itemIllustrationsRendered: {
        type: Number,
        value: 0,
      },
    };
  }

  private perksList: PerkData[];
  private selectedPerks: Set<string>;
  private currentPerk: number;
  private itemIconsRendered: number;
  private itemIllustrationsRendered: number;

  override get UI_STEPS() {
    return PerksDiscoveryStep;
  }

  static get observers(): string[] {
    return [
      'itemContentRendered(itemIconsRendered, itemIllustrationsRendered)',
    ];
  }


  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return PerksDiscoveryStep.LOADING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('PerksDiscoveryScreenScreen');
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setPerksData',
      'setOverviewStep',
    ];
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.perksList = [];
    this.currentPerk = -1;
    this.itemIconsRendered = 0;
    this.itemIllustrationsRendered = 0;
    if(this.selectedPerks) {
      this.selectedPerks.clear();
    }
  }

  isElementHidden(currentPerk: number, index: number): boolean {
    return !(currentPerk === index);
  }

  setPerksData(perksData: PerkData[]): void {
    assert(perksData !== null);
    this.perksList = perksData;
    this.selectedPerks = new Set<string>();
  }

  setOverviewStep(): void {
    this.currentPerk = 0;
    this.setUIStep(PerksDiscoveryStep.OVERVIEW);
  }

  private itemContentRendered(
      itemIconsRendered: number, itemIllustrationsRendered: number) {
    if (this.perksList.length === 0) {
      return;
    }

    if (itemIconsRendered === this.perksList.length) {
      this.setIconsWebviewStyle();
    }

    if (itemIllustrationsRendered === this.perksList.length) {
      this.setIllustrationWebviewStyle();
    }

    if (itemIconsRendered === this.perksList.length &&
        itemIllustrationsRendered === this.perksList.length) {
      this.userActed(UserAction.LOADED);
    }
  }

  /**
   * Dynamically styles illustration webviews based on data from `perksList`.
   * It sets the width and height of each webview using corresponding values
   * from the `perksList` array, ensuring that illustrations are displayed with
   * their intended dimensions.
   */
  private setIllustrationWebviewStyle(): void {
    const iconWebviews =
        this.shadowRoot?.querySelectorAll<chrome.webviewTag.WebView>(
            '.illustration');
    if (iconWebviews) {
      iconWebviews.forEach(
          (iconWebview: chrome.webviewTag.WebView, index: number) => {
            iconWebview.style['width'] =
                this.perksList[index].illustrationWidth;
            iconWebview.style['height'] =
                this.perksList[index].illustrationHeight;
            this.injectCss(iconWebview);
          });
    }
  }

  /**
   * Sets the background color of category icon webviews to match the system's
   * base shaded color.
   */
  private setIconsWebviewStyle(): void {
    const iconWebviews =
        this.shadowRoot?.querySelectorAll<chrome.webviewTag.WebView>(
            '.perk-icon');
    if (iconWebviews) {
      for (const iconWebview of iconWebviews) {
        this.injectCss(iconWebview);
      }
    }
  }

  /**
   * Injects CSS into a webview after its content has loaded.
   */
  private injectCss(webview: chrome.webviewTag.WebView) {
    webview.addEventListener('contentload', () => {
      webview.insertCSS(GENERATE_WEB_VIEW_CSS(), () => {
        if (chrome.runtime.lastError) {
          console.warn(
              'Failed to insertCSS: ' + chrome.runtime.lastError.message);
        }
      });
    });
  }

  private onBackClicked(): void {
    assert(this.currentPerk > 0);
    this.currentPerk--;
  }

  private canGoBack(currentStep: number): boolean {
    return currentStep > 0;
  }

  private advanceToNextPerk(): void {
    if (this.currentPerk === this.perksList.length - 1) {
      this.userActed([UserAction.FINISHED, Array.from(this.selectedPerks)]);
      return;
    }
    this.currentPerk++;
  }

  private onNotInterestedClicked(): void {
    this.selectedPerks.delete(this.perksList[this.currentPerk].perkId);
    this.advanceToNextPerk();
  }

  private onInterestedClicked(): void {
    this.selectedPerks.add(this.perksList[this.currentPerk].perkId);
    this.advanceToNextPerk();
  }


  /**
   * Returns the title of the perk.
   */
  private getCurrentPerkTitle(currentPerk: number): string {
    if (currentPerk === -1) {
      return '';
    }
    assert(currentPerk >= 0 && currentPerk < this.perksList.length);
    return this.perksList[currentPerk].title;
  }

  /**
   * Returns the subtitle of the perk.
   */
  private getCurrentPerkSubtitle(currentPerk: number): string {
    if (currentPerk === -1) {
      return '';
    }
    assert(currentPerk >= 0 && currentPerk < this.perksList.length);
    return this.perksList[currentPerk].subtitle;
  }

  /**
   * Returns the additional description of the perk.
   */
  private getCurrentPerkAdditionalText(currentPerk: number): string {
    if (currentPerk === -1) {
      return '';
    }
    assert(currentPerk >= 0 && currentPerk < this.perksList.length);
    if (!this.perksList[currentPerk].additionalText) {
      return '';
    }
    return this.perksList[currentPerk].additionalText;
  }

  /**
   * Returns the primary_button_label of the perk.
   */
  private getCurrentPerkPrimaryButtonLabel(currentPerk: number): string {
    if (currentPerk === -1) {
      return '';
    }
    assert(currentPerk >= 0 && currentPerk < this.perksList.length);
    return this.perksList[currentPerk].primaryButtonLabel;
  }

  /**
   * Returns the secondary_button_label of the perk.
   */
  private getCurrentPerkSecondaryButtonLabel(currentPerk: number): string {
    if (currentPerk === -1) {
      return '';
    }
    assert(currentPerk >= 0 && currentPerk < this.perksList.length);
    return this.perksList[currentPerk].secondaryButtonLabel;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [PerksDiscoveryElement.is]: PerksDiscoveryElement;
  }
}

customElements.define(PerksDiscoveryElement.is, PerksDiscoveryElement);
