// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/cr_components/customize_themes/customize_themes.js';
import './customize_backgrounds.js';
import './customize_shortcuts.js';
import './customize_modules.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeDialogPage} from './customize_dialog_types.js';
import {I18nBehavior, loadTimeData} from './i18n_setup.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {createScrollBorders} from './utils.js';


/**
 * Workaround until new_tab_page is migrated to TypeScript.
 * @interface
 */
class CustomizeThemesElement {
  revertThemeChanges() {}
  confirmThemeChanges() {}
}

/**
 * Dialog that lets the user customize the NTP such as the background color or
 * image.
 * @polymer
 * @extends {PolymerElement}
 */
class CustomizeDialogElement extends mixinBehaviors
([I18nBehavior], PolymerElement) {
  static get is() {
    return 'ntp-customize-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!newTabPage.mojom.Theme} */
      theme: Object,

      /** @type {CustomizeDialogPage} */
      selectedPage: {
        type: String,
        observer: 'onSelectedPageChange_',
      },

      /** @private {newTabPage.mojom.BackgroundCollection} */
      selectedCollection_: Object,

      /** @private */
      showTitleNavigation_: {
        type: Boolean,
        computed:
            'computeShowTitleNavigation_(selectedPage, selectedCollection_)',
        value: false,
      },

      /** @private */
      isRefreshToggleChecked_: {
        type: Boolean,
        computed: `computeIsRefreshToggleChecked_(theme, selectedCollection_)`,
      },

      /** @private */
      shortcutsEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('shortcutsEnabled'),
      },

      /** @private */
      modulesEnabled_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('modulesEnabled'),
      },
    };
  }

  constructor() {
    super();
    /** @private {newTabPage.mojom.PageHandlerRemote} */
    this.pageHandler_ = NewTabPageProxy.getInstance().handler;
    /** @private {!Array<!IntersectionObserver>} */
    this.intersectionObservers_ = [];
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    this.intersectionObservers_.forEach(observer => {
      observer.disconnect();
    });
    this.intersectionObservers_ = [];
  }

  /** @override */
  ready() {
    super.ready();
    this.intersectionObservers_ = [
      createScrollBorders(
          this.$.menu, this.$.topPageScrollBorder,
          this.$.bottomPageScrollBorder, 'show-1'),
      createScrollBorders(
          this.$.pages, this.$.topPageScrollBorder,
          this.$.bottomPageScrollBorder, 'show-2'),
    ];
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction.kOpenClicked);
  }

  /** @private */
  onCancel_() {
    this.$.backgrounds.revertBackgroundChanges();
    /** @type {CustomizeThemesElement} */ (this.$.customizeThemes)
        .revertThemeChanges();
  }

  /** @private */
  onCancelClick_() {
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction.kCancelClicked);
    this.$.dialog.cancel();
  }

  /**
   * @private
   */
  onDoneClick_() {
    this.$.backgrounds.confirmBackgroundChanges();
    /** @type {CustomizeThemesElement} */ (this.$.customizeThemes)
        .confirmThemeChanges();
    this.shadowRoot.querySelector('ntp-customize-shortcuts').apply();
    if (this.modulesEnabled_) {
      this.shadowRoot.querySelector('ntp-customize-modules').apply();
    }
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction.kDoneClicked);
    this.$.dialog.close();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onMenuItemKeyDown_(e) {
    if (!['Enter', ' '].includes(e.key)) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    this.selectedPage = e.target.getAttribute('page-name');
  }

  /** @private */
  onSelectedPageChange_() {
    this.$.pages.scrollTop = 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeIsRefreshToggleChecked_() {
    if (!this.selectedCollection_) {
      return false;
    }
    return !!this.theme &&
        this.selectedCollection_.id === this.theme.dailyRefreshCollectionId;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShowTitleNavigation_() {
    return this.selectedPage === CustomizeDialogPage.BACKGROUNDS &&
        !!this.selectedCollection_;
  }

  /** @private */
  onBackClick_() {
    this.selectedCollection_ = null;
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction.kBackgroundsBackClicked);
    this.$.pages.scrollTop = 0;
  }

  /** @private */
  onBackgroundDailyRefreshToggleChange_() {
    if (this.$.refreshToggle.checked) {
      this.pageHandler_.setDailyRefreshCollectionId(
          this.selectedCollection_.id);
    } else {
      this.pageHandler_.setDailyRefreshCollectionId('');
    }
    this.pageHandler_.onCustomizeDialogAction(
        newTabPage.mojom.CustomizeDialogAction
            .kBackgroundsRefreshToggleClicked);
  }
}

customElements.define(CustomizeDialogElement.is, CustomizeDialogElement);
