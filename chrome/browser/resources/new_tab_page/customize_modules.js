// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy} from './browser_proxy.js';
import {ModuleRegistry} from './modules/module_registry.js';

/** Element that lets the user configure modules settings. */
class CustomizeModulesElement extends PolymerElement {
  static get is() {
    return 'ntp-customize-modules';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * If true, modules are customizable. If false, all modules are hidden.
       * @private
       */
      show_: {
        type: Boolean,
        observer: 'onShowChange_',
      },

      /** @private */
      showManagedByPolicy_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesVisibleManagedByPolicy'),
      },

      /**
       * @private {
       *   !Array<{
       *     name: string,
       *     id: string,
       *     checked: boolean,
       *     initiallyChecked: boolean,
       *     disabled: boolean,
       *   }>
       * }
       */
      modules_: {
        type: Array,
        value: () => ModuleRegistry.getInstance().getDescriptors().map(
            d => ({name: d.name, id: d.id, checked: true, hidden: false})),
      },
    };
  }

  constructor() {
    super();
    /** @private {?number} */
    this.setDisabledModulesListenerId_ = null;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setDisabledModulesListenerId_ =
        BrowserProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener((all, ids) => {
              this.show_ = !all;
              this.modules_.forEach(({id}, i) => {
                const checked = !all && !ids.includes(id);
                this.set(`modules_.${i}.checked`, checked);
                this.set(`modules_.${i}.initiallyChecked`, checked);
                this.set(`modules_.${i}.disabled`, ids.includes(id));
              });
            });
    BrowserProxy.getInstance().handler.updateDisabledModules();
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    BrowserProxy.getInstance().callbackRouter.removeListener(
        assert(this.setDisabledModulesListenerId_));
  }

  /** @override */
  ready() {
    // |window.CrPolicyStrings.controlledSettingPolicy| populates the tooltip
    // text of <cr-policy-indicator indicator-type="devicePolicy" /> elements.
    // Needs to be called before |super.ready()| so that the string is available
    // when <cr-policy-indicator> gets instantiated.
    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('controlledSettingPolicy'),
    };
    super.ready();
  }

  apply() {
    const handler = BrowserProxy.getInstance().handler;
    handler.setModulesVisible(this.show_);
    this.modules_
        .filter(({checked, initiallyChecked}) => checked !== initiallyChecked)
        .forEach(({id, checked}) => {
          // Don't set disabled status of a module if we are in hide all mode to
          // preserve the status for the next time we go into customize mode.
          if (this.show_) {
            handler.setModuleDisabled(id, !checked);
          }
          const base = `NewTabPage.Modules.${checked ? 'Enabled' : 'Disabled'}`;
          chrome.metricsPrivate.recordSparseHashable(base, id);
          chrome.metricsPrivate.recordSparseHashable(`${base}.Customize`, id);
        });
  }

  /**
   * @param {!CustomEvent<{value: string}>} e
   * @private
   */
  onShowRadioSelectionChanged_(e) {
    this.show_ = e.detail.value === 'customize';
  }

  /** @private */
  onShowChange_() {
    this.modules_.forEach(
        (m, i) => this.set(`modules_.${i}.checked`, this.show_ && !m.disabled));
  }

  /**
   * @return {string}
   * @private
   */
  radioSelection_() {
    return this.show_ ? 'customize' : 'hide';
  }

  /**
   * @return {boolean}
   * @private
   */
  moduleToggleDisabled_() {
    return this.showManagedByPolicy_ || !this.show_;
  }
}

customElements.define(CustomizeModulesElement.is, CustomizeModulesElement);
