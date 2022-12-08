// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '//resources/ash/common/cr_scrollable_behavior.js';
import '//resources/cr_elements/icons.html.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nBehavior, OobeI18nBehaviorInterface} from './behaviors/oobe_i18n_behavior.js';


const MAX_IMG_LOADING_TIME_SEC = 7;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 */
const OobeAppsListBase = mixinBehaviors([OobeI18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class OobeAppsList extends OobeAppsListBase {
  static get is() {
    return 'oobe-apps-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Apps list.
       */
      appList: {
        type: Array,
        value: [],
        observer: 'onAppListSet_',
      },

      appsSelected: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  constructor() {
    super();
    /**
     * Timer id of pending load.
     * @type {number|undefined}
     * @private
     */
    this.loadingTimer_ = undefined;
    this.allSelected_ = false;
    this.loadedImagesCount_ = 0;
  }

  /**
   * Clears loading timer.
   * @private
   */
  clearLoadingTimer_() {
    if (this.loadingTimer_) {
      clearTimeout(this.loadingTimer_);
      this.loadingTimer_ = undefined;
    }
  }

  /**
   * Called when app list is changed. We assume that non-empty list is set only
   * once.
   * @private
   */
  onAppListSet_() {
    if (this.appList.length === 0) {
      return;
    }
    this.clearLoadingTimer_();
    // Wait a few seconds before at least some icons are downloaded. If it
    // happens faster we will exit waiting timer.
    this.loadingTimer_ = setTimeout(
        this.onLoadingTimeOut_.bind(this), MAX_IMG_LOADING_TIME_SEC * 1000);
  }

  /**
   * Handler for icons loading timeout.
   * @private
   */
  onLoadingTimeOut_() {
    this.loadingTimer_ = undefined;
    this.dispatchEvent(
        new CustomEvent('apps-list-loaded', {bubbles: true, composed: true}));
  }

  /**
   * Wrap the icon as a image into a html snippet.
   *
   * @param {string} iconUri the icon uri to be wrapped.
   * @return {string} wrapped html snippet.
   *
   * @private
   */
  getWrappedIcon_(iconUri) {
    return ('data:text/html;charset=utf-8,' + encodeURIComponent(String.raw`
    <html>
      <style>
        body {
          margin: 0;
        }
        #icon {
          width: 48px;
          height: 48px;
          user-select: none;
        }
      </style>
    <body><img id="icon" src="` + iconUri + '"></body></html>'));
  }

  /**
   * @param {string} title the name of the application.
   * @return {string} aria label for the expand button.
   * @private
   */
  getExpandButtonAriaLabel_(title) {
    return this.i18n('recommendAppsDescriptionExpand', title);
  }

  /**
   * After any change in selection update current counter.
   * @private
   */
  updateCount_() {
    let appsSelected = 0;
    this.appList.forEach((app) => {
      appsSelected += app.checked;
    });
    this.appsSelected = appsSelected;
    this.allSelected_ = this.appsSelected === this.appList.length;
  }

  /**
   * Set all checkboxes to a new value.
   * @param {boolean} value new state for all checkboxes
   * @private
   */
  updateSelectionTo_(value) {
    this.appList.forEach((_, index) => {
      this.set('appList.' + index + '.checked', value);
    });
  }

  /**
   * Called when select all checkbox is clicked. It has 3 states:
   *  1) all are selected;
   *  2) some are selected;
   *  3) nothing is selected.
   * Clicking the button in states 1 and 2 leads to a state 3; from state 3 to
   * state 1.
   * @private
   */
  updateSelection_() {
    if (this.allSelected_ && this.appsSelected === 0) {
      this.updateSelectionTo_(true);
    } else {
      this.updateSelectionTo_(false);
    }
    // When there are selected apps clicking on this checkbox should unselect
    // them. Keep checkbox unchecked.
    if (this.allSelected_ && this.appsSelected > 0) {
      this.allSelected_ = false;
    }
    this.updateCount_();
  }

  /**
   * @param {!Event} e
   * @private
   */
  onExpandClicked_(e) {
    const appItem = e.currentTarget.parentElement.parentElement;
    appItem.querySelector('.app-description').classList.toggle('truncated');
  }

  /**
   * Called when single webview loads app icon.
   * @private
   */
  onImageLoaded_() {
    this.loadedImagesCount_ += 1;
    if (this.loadedImagesCount_ === this.appList.length) {
      this.clearLoadingTimer_();

      this.dispatchEvent(
          new CustomEvent('apps-list-loaded', {bubbles: true, composed: true}));
    }
  }

  /**
   * Return a list of selected apps.
   * @returns {!Array<String>}
   */
  getSelectedApps() {
    const packageNames = [];
    this.appList.forEach((app) => {
      if (app.checked) {
        packageNames.push(app.package_name);
      }
    });
    return packageNames;
  }

  unselectSymbolHidden_(appsSelected) {
    if (appsSelected > 0 && appsSelected < this.appList.length) {
      this.$.selectAll.classList.add('some-selected');
      return false;
    } else {
      this.$.selectAll.classList.remove('some-selected');
      return true;
    }
  }
}

customElements.define(OobeAppsList.is, OobeAppsList);
