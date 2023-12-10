// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/shared/settings/controls/settings_toggle_button.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
// <if expr="not chromeos_ash">
import '../relaunch_confirmation_dialog.js';

// </if>

import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {RelaunchMixin, RestartType} from '../relaunch_mixin.js';

import {getTemplate} from './ai_page.html.js';

// These values must stay in sync with
// optimization_guide::prefs::FeatureOptInState in
// components/optimization_guide/core/optimization_guide_prefs.h.
export enum FeatureOptInState {
  NOT_INITIALIZED = 0,
  ENABLED = 1,
  DISABLED = 2,
}

// Exporting pref names so that they can be referenced by tests.
export enum SettingsAiPageFeaturePrefName {
  MAIN = 'optimization_guide.model_execution_main_toggle_setting_state',
  COMPOSE = 'optimization_guide.compose_setting_state',
  TAB_ORGANIZATION = 'optimization_guide.tab_organization_setting_state',
  WALLPAPER_SEARCH = 'optimization_guide.wallpaper_search_setting_state',
}

export interface SettingsAiPageElement {
  $: {
    toast: CrToastElement,
  };
}

const SettingsAiPageElementBase = RelaunchMixin(PrefsMixin(PolymerElement));

export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';
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

      showComposeControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showComposeControl'),
      },

      showTabOrganizationControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showTabOrganizationControl'),
      },

      showWallpaperSearchControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showWallpaperSearchControl'),
      },

      featureOptInStateEnum_: {
        type: Object,
        value: FeatureOptInState,
      },

      numericUncheckedValues_: {
        type: Array,
        value: () =>
            [FeatureOptInState.DISABLED, FeatureOptInState.NOT_INITIALIZED],
      },
    };
  }

  private showComposeControl_: boolean;
  private showTabOrganizationControl_: boolean;
  private showWallpaperSearchControl_: boolean;
  private numericUncheckedValues_: FeatureOptInState[];

  private onToggleChange_() {
    this.$.toast.show();
  }

  private onRestartClick_(e: Event) {
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }

  private isExpanded_(): boolean {
    return this.getPref(SettingsAiPageFeaturePrefName.MAIN).value ===
        FeatureOptInState.ENABLED;
  }

  private getTabOrganizationHrCssClass_(): string {
    return this.showComposeControl_ ? 'hr' : '';
  }

  private getWallpaperSearchHrCssClass_(): string {
    return this.showComposeControl_ || this.showTabOrganizationControl_ ? 'hr' :
                                                                          '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page': SettingsAiPageElement;
  }
}

customElements.define(SettingsAiPageElement.is, SettingsAiPageElement);
