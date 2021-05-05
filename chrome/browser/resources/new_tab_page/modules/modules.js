// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './module_wrapper.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior, loadTimeData} from '../i18n_setup.js';
import {NewTabPageProxy} from '../new_tab_page_proxy.js';
import {$$} from '../utils.js';

import {Module} from './module_descriptor.js';
import {ModuleRegistry} from './module_registry.js';

/**
 * Container for the NTP modules.
 * @polymer
 * @extends {PolymerElement}
 */
export class ModulesElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-modules';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private {!Array<!Module>} */
      modules_: Object,

      /** @private {!Array<string>} */
      dismissedModules_: {
        type: Array,
        value: () => [],
      },

      /** @private {!{all: boolean, ids: !Array<string>}} */
      disabledModules_: {
        type: Object,
        value: () => ({all: true, ids: []}),
      },

      /**
       * Data about the most recently removed module.
       * @type {?{message: string, undo: function()}}
       * @private
       */
      removedModuleData_: {
        type: Object,
        value: null,
      },

      /** @private */
      modulesLoaded_: Boolean,

      /** @private */
      modulesVisibilityDetermined_: Boolean,

      /** @private */
      modulesLoadedAndVisibilityDetermined_: {
        type: Boolean,
        computed: `computeModulesLoadedAndVisibilityDetermined_(
          modulesLoaded_,
          modulesVisibilityDetermined_)`,
        observer: 'onModulesLoadedAndVisibilityDeterminedChange_',
      },
    };
  }

  constructor() {
    super();
    /** @private {?number} */
    this.setDisabledModulesListenerId_ = null;
    /** @private {!EventTracker} */
    this.eventTracker_ = new EventTracker();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.setDisabledModulesListenerId_ =
        NewTabPageProxy.getInstance()
            .callbackRouter.setDisabledModules.addListener((all, ids) => {
              this.disabledModules_ = {all, ids};
              this.modulesVisibilityDetermined_ = true;
            });
    NewTabPageProxy.getInstance().handler.updateDisabledModules();
    this.eventTracker_.add(window, 'keydown', e => this.onWindowKeydown_(e));
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    NewTabPageProxy.getInstance().callbackRouter.removeListener(
        assert(this.setDisabledModulesListenerId_));
    this.eventTracker_.removeAll();
  }

  /** @override */
  ready() {
    super.ready();
    this.renderModules_();
  }

  /**
   * @return {!Promise<void>}
   * @private
   */
  async renderModules_() {
    this.modules_ = await ModuleRegistry.getInstance().initializeModules(
        loadTimeData.getInteger('modulesLoadTimeout'));
    if (this.modules_) {
      NewTabPageProxy.getInstance().handler.onModulesLoadedWithData();
    }
  }

  /**
   * @param {KeyboardEvent} e
   * @private
   */
  onWindowKeydown_(e) {
    let ctrlKeyPressed = e.ctrlKey;
    // <if expr="is_macosx">
    ctrlKeyPressed = ctrlKeyPressed || e.metaKey;
    // </if>
    if (ctrlKeyPressed && e.key === 'z') {
      this.onUndoRemoveModuleButtonClick_();
    }
  }

  /** @private */
  onModulesLoaded_() {
    this.modulesLoaded_ = true;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeModulesLoadedAndVisibilityDetermined_() {
    return this.modulesLoaded_ && this.modulesVisibilityDetermined_;
  }

  /** @private */
  onModulesLoadedAndVisibilityDeterminedChange_() {
    if (this.modulesLoadedAndVisibilityDetermined_) {
      this.modules_.forEach(({descriptor: {id}}) => {
        chrome.metricsPrivate.recordBoolean(
            `NewTabPage.Modules.EnabledOnNTPLoad.${id}`,
            !this.disabledModules_.all &&
                !this.disabledModules_.ids.includes(id));
      });
      chrome.metricsPrivate.recordBoolean(
          'NewTabPage.Modules.VisibleOnNTPLoad', !this.disabledModules_.all);
      this.dispatchEvent(new Event('modules-loaded'));
    }
  }

  /**
   * @param {!CustomEvent<{message: string, restoreCallback: function()}>} e
   *     Event notifying a module was dismissed. Contains the message to show in
   *     the toast.
   * @private
   */
  onDismissModule_(e) {
    const id = $$(this, '#modules').itemForElement(e.target).descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.removedModuleData_ = {
      message: e.detail.message,
      undo: () => {
        this.splice('dismissedModules_', this.dismissedModules_.indexOf(id), 1);
        restoreCallback();
        NewTabPageProxy.getInstance().handler.onRestoreModule(id);
      },
    };
    if (!this.dismissedModules_.includes(id)) {
      this.push('dismissedModules_', id);
    }

    // Notify the user.
    $$(this, '#removeModuleToast').show();
    // Notify the backend.
    NewTabPageProxy.getInstance().handler.onDismissModule(id);
  }

  /**
   * @param {!CustomEvent<{message: string, restoreCallback: ?function()}>} e
   *     Event notifying a module was disabled. Contains the message to show in
   *     the toast.
   * @private
   */
  onDisableModule_(e) {
    const id = $$(this, '#modules').itemForElement(e.target).descriptor.id;
    const restoreCallback = e.detail.restoreCallback;
    this.removedModuleData_ = {
      message: e.detail.message,
      undo: () => {
        if (restoreCallback) {
          restoreCallback();
        }
        NewTabPageProxy.getInstance().handler.setModuleDisabled(id, false);
        chrome.metricsPrivate.recordSparseHashable(
            'NewTabPage.Modules.Enabled', id);
        chrome.metricsPrivate.recordSparseHashable(
            'NewTabPage.Modules.Enabled.Toast', id);
      },
    };

    NewTabPageProxy.getInstance().handler.setModuleDisabled(id, true);
    $$(this, '#removeModuleToast').show();
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled', id);
    chrome.metricsPrivate.recordSparseHashable(
        'NewTabPage.Modules.Disabled.ModuleRequest', id);
  }

  /**
   * @param {string} id
   * @return {boolean}
   * @private
   */
  moduleDisabled_(id) {
    return this.disabledModules_.all || this.dismissedModules_.includes(id) ||
        this.disabledModules_.ids.includes(id);
  }

  /** @private */
  onUndoRemoveModuleButtonClick_() {
    if (!this.removedModuleData_) {
      return;
    }

    // Restore the module.
    this.removedModuleData_.undo();

    // Notify the user.
    $$(this, '#removeModuleToast').hide();

    this.removedModuleData_ = null;
  }
}

customElements.define(ModulesElement.is, ModulesElement);
