// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Recommend Apps
 * screen.
 */

import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_apps_list.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeAppsList} from '../../components/oobe_apps_list.js';

import {getTemplate} from './recommend_apps.html.js';

enum RecommendAppsUiState {
  LOADING = 'loading',
  LIST = 'list',
}

interface AppFetchedData {
  title: string;
  icon_url: string;
  category: string;
  in_app_purchases: boolean;
  was_installed: boolean;
  content_rating: string;
  description: string;
  contains_ads: boolean;
  package_name: string;
}

interface AppData {
  title: string;
  icon_url: string;
  tags: string[];
  description: string;
  package_name: string;
  checked: boolean;
}

const RecommendAppsElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

class RecommendAppsElement extends RecommendAppsElementBase {
  static get is() {
    return 'recommend-apps-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      appsSelected: {
        type: Number,
        value: 0,
      },

      appList: {
        type: Array,
        value: [],
      },
    };
  }

  appsSelected: number;
  appList: AppData[];
  initialized: boolean;

  constructor() {
    super();
    this.initialized = false;
  }

  override get EXTERNAL_API(): string[] {
    return ['loadAppList'];
  }

  override get UI_STEPS() {
    return RecommendAppsUiState;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('RecommendAppsScreen');
  }

  /**
   * Resets screen to initial state.
   * Currently is used for debugging purposes only.
   */
  reset(): void {
    this.setUIStep(RecommendAppsUiState.LOADING);
    this.appsSelected = 0;
    this.appList = [];
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    const appsDialog = this.shadowRoot?.querySelector<HTMLElement>(
      '#appsDialog');
    if (appsDialog instanceof HTMLElement) {
      return appsDialog;
    }
    return null;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return RecommendAppsUiState.LOADING;
  }

  /**
   * Initial UI State for screen
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.appList = [];
  }

  /**
   * Generates the contents in the webview.
   */
  loadAppList(appList: AppFetchedData[]): void {
    const recommendAppsContainsAdsStr = this.i18n('recommendAppsContainsAds');
    const recommendAppsInAppPurchasesStr =
        this.i18n('recommendAppsInAppPurchases');
    const recommendAppsWasInstalledStr = this.i18n('recommendAppsWasInstalled');
    this.appList = appList.map((app: AppFetchedData) => {
      const tagList = [app.category];
      if (app.contains_ads) {
        tagList.push(recommendAppsContainsAdsStr);
      }
      if (app.in_app_purchases) {
        tagList.push(recommendAppsInAppPurchasesStr);
      }
      if (app.was_installed) {
        tagList.push(recommendAppsWasInstalledStr);
      }
      if (app.content_rating) {
        tagList.push(app.content_rating);
      }
      return {
        title: app.title,
        icon_url: app.icon_url,
        tags: tagList,
        description: app.description,
        package_name: app.package_name,
        checked: false,
      };
    });
  }

  /**
   * Handles event when contents in the webview is generated.
   */
  private onFullyLoaded(): void {
    this.setUIStep(RecommendAppsUiState.LIST);
    const appsList = this.shadowRoot?.querySelector<HTMLElement>(
      '#appsList');
    if (appsList instanceof HTMLElement) {
      appsList.focus();
    }
  }

  /**
   * Handles Skip button click.
   */
  private onSkip(): void {
    this.userActed('recommendAppsSkip');
  }

  /**
   * Handles Install button click.
   */
  private onInstall(): void {
    // Button should be disabled if nothing is selected.
    assert(this.appsSelected > 0);
    const appsList = this.shadowRoot?.querySelector(
      '#appsList');
    if (appsList instanceof OobeAppsList) {
      const packageNames = appsList.getSelectedApps();
      this.userActed(['recommendAppsInstall', packageNames]);
    }
  }

  private canProceed(appsSelected: number): boolean {
    return appsSelected > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [RecommendAppsElement.is]: RecommendAppsElement;
  }
}

customElements.define(RecommendAppsElement.is, RecommendAppsElement);
