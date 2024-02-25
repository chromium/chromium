// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toggle_row.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppManagementUserAction} from 'chrome://resources/cr_components/app_management/constants.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../../assert_extras.js';
import {AppManagementBrowserProxy} from '../../common/app_management/browser_proxy.js';
import {recordSettingChange} from '../../metrics_recorder.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './resize_lock_item.html.js';
import {AppManagementToggleRowElement} from './toggle_row.js';

export class AppManagementResizeLockItemElement extends PolymerElement {
  static get is() {
    return 'app-management-resize-lock-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      app: Object,

      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },
    };
  }

  app: App;
  override hidden: boolean;

  override ready(): void {
    super.ready();

    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleSetting_);
  }

  private getValue_(app: App): boolean {
    return app.resizeLocked;
  }

  private isHidden_(app: App): boolean {
    return app.hideResizeLocked;
  }

  private toggleSetting_(): void {
    const newState = !this.app.resizeLocked;
    assert(newState === this.getToggleRow_().isChecked());
    AppManagementBrowserProxy.getInstance().handler.setResizeLocked(
        this.app.id,
        newState,
    );
    recordSettingChange(Setting.kAppResizeLockOnOff, {boolValue: newState});
    const userAction = newState ?
        AppManagementUserAction.RESIZE_LOCK_TURNED_ON :
        AppManagementUserAction.RESIZE_LOCK_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, userAction);
  }

  private onClick_(): void {
    this.getToggleRow_().click();
  }

  private getToggleRow_(): AppManagementToggleRowElement {
    return castExists(
        this.shadowRoot!.querySelector<AppManagementToggleRowElement>(
            '#toggleRow'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'app-management-resize-lock-item': AppManagementResizeLockItemElement;
  }
}

customElements.define(
    AppManagementResizeLockItemElement.is, AppManagementResizeLockItemElement);
