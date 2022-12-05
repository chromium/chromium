// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_dialog.js';
import './shortcut_input.js';
import './shortcuts_page.js';
import '../strings.m.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorEditDialogElement} from './accelerator_edit_dialog.js';
import {RequestUpdateAcceleratorEvent} from './accelerator_edit_view.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ShowEditDialogEvent} from './accelerator_row.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {getTemplate} from './shortcut_customization_app.html.js';
import {AcceleratorInfo, AcceleratorSource, AcceleratorState, AcceleratorType, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';
import {getCategoryNameStringId, isCustomizationDisabled} from './shortcut_utils.js';

export interface ShortcutCustomizationAppElement {
  $: {
    navigationPanel: NavigationViewPanelElement,
  };
}

declare global {
  interface HTMLElementEventMap {
    'edit-dialog-closed': CustomEvent<void>;
    'request-update-accelerator': RequestUpdateAcceleratorEvent;
    'show-edit-dialog': ShowEditDialogEvent;
  }
}

/**
 * @fileoverview
 * 'shortcut-customization-app' is the main landing page for the shortcut
 * customization app.
 */

const ShortcutCustomizationAppElementBase = I18nMixin(PolymerElement);

export class ShortcutCustomizationAppElement extends
    ShortcutCustomizationAppElementBase {
  static get is() {
    return 'shortcut-customization-app';
  }

  static get properties() {
    return {
      dialogShortcutTitle_: {
        type: String,
        value: '',
      },

      dialogAccelerators_: {
        type: Array,
        value: () => {},
      },

      dialogAction_: {
        type: Number,
        value: 0,
      },

      dialogSource_: {
        type: Number,
        value: 0,
      },

      showEditDialog_: {
        type: Boolean,
        value: false,
      },

      showRestoreAllDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  protected showRestoreAllDialog_: boolean;
  protected dialogShortcutTitle_: string;
  protected dialogAccelerators_: AcceleratorInfo[];
  protected dialogAction_: number;
  protected dialogSource_: AcceleratorSource;
  protected showEditDialog_: boolean;
  private shortcutProvider_: ShortcutProviderInterface = getShortcutProvider();
  private acceleratorLookupManager_: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.fetchAccelerators_();
    this.addEventListener('show-edit-dialog', this.showDialog_);
    this.addEventListener('edit-dialog-closed', this.onDialogClosed_);
    this.addEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('show-edit-dialog', this.showDialog_);
    this.removeEventListener('edit-dialog-closed', this.onDialogClosed_);
    this.removeEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators_);
  }

  private fetchAccelerators_() {
    // Kickoff fetching accelerators by first fetching the accelerator configs.
    this.shortcutProvider_.getAccelerators().then(
        ({config}) => this.onAcceleratorConfigFetched_(config));
  }

  private onAcceleratorConfigFetched_(config: MojoAcceleratorConfig) {
    this.acceleratorLookupManager_.setAcceleratorLookup(config);
    // After fetching the config infos, fetch the layout infos next.
    this.shortcutProvider_.getAcceleratorLayoutInfos().then(
        ({layoutInfos}) => this.onLayoutInfosFetched_(layoutInfos));
  }

  private onLayoutInfosFetched_(layoutInfos: MojoLayoutInfo[]): void {
    this.addNavigationSelectors_(layoutInfos);
    this.acceleratorLookupManager_.setAcceleratorLayoutLookup(layoutInfos);
    // Notify pages to update their accelerators.
    this.$.navigationPanel.notifyEvent('updateAccelerators');
  }

  private addNavigationSelectors_(layoutInfos: MojoLayoutInfo[]): void {
    // A Set is used here to remove duplicates from the array of categories.
    const uniqueCategoriesInOrder =
        new Set(layoutInfos.map(layoutInfo => layoutInfo.category));
    const pages = Array.from(uniqueCategoriesInOrder).map(category => {
      const categoryNameStringId = getCategoryNameStringId(category);
      const categoryName = this.i18n(categoryNameStringId);
      return this.$.navigationPanel.createSelectorItem(
          categoryName, 'shortcuts-page', '', `${categoryNameStringId}-page-id`,
          {category});
    });
    this.$.navigationPanel.addSelectors(pages);
  }

  private showDialog_(e: ShowEditDialogEvent) {
    this.dialogShortcutTitle_ = e.detail.description;
    this.dialogAccelerators_ = e.detail.accelerators;
    this.dialogAction_ = e.detail.action;
    this.dialogSource_ = e.detail.source;
    this.showEditDialog_ = true;
  }

  private onDialogClosed_() {
    this.showEditDialog_ = false;
    this.dialogShortcutTitle_ = '';
    this.dialogAccelerators_ = [];
  }

  private onRequestUpdateAccelerators_(e: RequestUpdateAcceleratorEvent) {
    this.$.navigationPanel.notifyEvent('updateSubsections');
    const updatedAccels =
        this.acceleratorLookupManager_
            .getAcceleratorInfos(e.detail.source, e.detail.action)
            ?.filter((accel) => {
              // Hide accelerators that are default and disabled.
              return !(
                  accel.type === AcceleratorType.kDefault &&
                  accel.state === AcceleratorState.kDisabledByUser);
            });

    this.shadowRoot!.querySelector<AcceleratorEditDialogElement>('#editDialog')!
        .updateDialogAccelerators(updatedAccels as AcceleratorInfo[]);
  }

  protected onRestoreAllDefaultClicked_() {
    this.showRestoreAllDialog_ = true;
  }

  protected onCancelRestoreButtonClicked_() {
    this.closeRestoreAllDialog_();
  }

  protected onConfirmRestoreButtonClicked_() {
    // TODO(jimmyxgong): Implement this function.
  }

  protected closeRestoreAllDialog_() {
    this.showRestoreAllDialog_ = false;
  }

  protected shouldHideRestoreAllButton_() {
    return isCustomizationDisabled();
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'shortcut-customization-app': ShortcutCustomizationAppElement;
  }
}

customElements.define(
    ShortcutCustomizationAppElement.is, ShortcutCustomizationAppElement);
