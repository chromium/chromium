// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/ash/common/cr_scrollable_behavior.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import './common_styles/oobe_common_styles.css.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';
import {OobeScrollableMixin} from './mixins/oobe_scrollable_mixin.js';
import {getTemplate} from './oobe_apps_list.html.js';

const MAX_IMG_LOADING_TIME_SEC = 7;

const OobeAppsListBase = OobeI18nMixin(OobeScrollableMixin(PolymerElement));

export class OobeAppsList extends OobeAppsListBase {
  static get is() {
    return 'oobe-apps-list' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Apps list.
       */
      appList: {
        type: Array,
        value: [],
        observer: 'onAppListSet',
      },

      appsSelected: {
        type: Number,
        value: 0,
        notify: true,
      },
    };
  }

  appList: any[];
  appsSelected: number;
  private loadingTimer: number|undefined;
  allSelected: boolean;
  loadedImagesCount: number;

  constructor() {
    super();
    /**
     * Timer id of pending load.
     */
    this.loadingTimer = undefined;
    this.allSelected = false;
    this.loadedImagesCount = 0;
  }

  override ready() {
    super.ready();
    const scrollContainer = this.shadowRoot?.querySelector('#appsList');
    const scrollContent = this.shadowRoot?.querySelector('#scrollContent');
    if (!scrollContainer || !scrollContent) {
      return;
    }
    this.initScrollableObservers(scrollContainer, scrollContent);
  }

  /**
   * Clears loading timer.
   */
  private clearloadingTimer(): void {
    if (this.loadingTimer) {
      clearTimeout(this.loadingTimer);
      this.loadingTimer = undefined;
    }
  }

  /**
   * Called when app list is changed. We assume that non-empty list is set only
   * once.
   */
  private onAppListSet(): void {
    if (this.appList.length === 0) {
      return;
    }
    this.clearloadingTimer();
    // Wait a few seconds before at least some icons are downloaded. If it
    // happens faster we will exit waiting timer.
    this.loadingTimer = setTimeout(
        this.onLoadingTimeOut.bind(this), MAX_IMG_LOADING_TIME_SEC * 1000);
  }

  /**
   * Handler for icons loading timeout.
   */
  private onLoadingTimeOut(): void {
    this.loadingTimer = undefined;
    this.dispatchEvent(
        new CustomEvent('apps-list-loaded', {bubbles: true, composed: true}));
  }

  /**
   * Wrap the icon as a image into a html snippet.
   *
   * @param iconUri the icon uri to be wrapped.
   * @return wrapped html snippet.
   *
   */
  private getWrappedIcon(iconUri: string): string {
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
   * After any change in selection update current counter.
   */
  private updateCount(): void {
    let appsSelected = 0;
    this.appList.forEach((app) => {
      appsSelected += app.checked;
    });
    this.appsSelected = appsSelected;
    this.allSelected = this.appsSelected === this.appList.length;
  }

  /**
   * Set all checkboxes to a new value.
   * @param value new state for all checkboxes
   */
  private updateSelectionTo(value: boolean): void {
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
   */
  private updateSelection(): void {
    if (this.allSelected && this.appsSelected === 0) {
      this.updateSelectionTo(true);
    } else {
      this.updateSelectionTo(false);
    }
    // When there are selected apps clicking on this checkbox should unselect
    // them. Keep checkbox unchecked.
    if (this.allSelected && this.appsSelected > 0) {
      this.allSelected = false;
    }
    this.updateCount();
  }

  /**
   * Called when single webview loads app icon.
   */
  private onImageLoaded(): void {
    this.loadedImagesCount += 1;
    if (this.loadedImagesCount === this.appList.length) {
      this.clearloadingTimer();

      this.dispatchEvent(
          new CustomEvent('apps-list-loaded', {bubbles: true, composed: true}));
    }
  }

  /**
   * Return a list of selected apps.
   */
  getSelectedApps(): string[] {
    const packageNames: string[] = [];
    this.appList.forEach((app) => {
      if (app.checked) {
        packageNames.push(app.package_name);
      }
    });
    return packageNames;
  }

  unselectSymbolHidden(appsSelected: number): boolean {
    const selectAll = this.shadowRoot?.querySelector('#selectAll');
    // First call happens during creation of unselectAll, before the shadow tree
    // is attached to the dom
    if (!selectAll) {
      return true;
    }
    if (appsSelected > 0 && appsSelected < this.appList.length) {
      selectAll.classList.add('some-selected');
      return false;
    } else {
      selectAll.classList.remove('some-selected');
      return true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeAppsList.is]: OobeAppsList;
  }
}

customElements.define(OobeAppsList.is, OobeAppsList);
