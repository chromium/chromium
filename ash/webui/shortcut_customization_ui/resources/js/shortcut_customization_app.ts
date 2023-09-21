// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_dialog.js';
import './bottom_nav_content.js';
import './shortcut_input.js';
import './shortcuts_page.js';
import '../strings.m.js';
import './search/search_box.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/ash/common/navigation_view_panel.js';
import 'chrome://resources/ash/common/page_toolbar.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorsUpdatedObserverInterface, AcceleratorsUpdatedObserverReceiver, PolicyUpdatedObserverInterface, PolicyUpdatedObserverReceiver, UserAction} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';

import {AcceleratorEditDialogElement} from './accelerator_edit_dialog.js';
import {RequestUpdateAcceleratorEvent} from './accelerator_edit_view.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {ShowEditDialogEvent} from './accelerator_row.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {RouteObserver, Router} from './router.js';
import {getTemplate} from './shortcut_customization_app.html.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorSource, MojoAcceleratorConfig, MojoLayoutInfo, ShortcutProviderInterface} from './shortcut_types.js';
import {getAcceleratorId, getCategoryNameStringId, isCustomizationAllowed} from './shortcut_utils.js';

const oldKeyboardSettingsLink = 'chrome://os-settings/keyboard-overlay';
const newKeyboardSettingsLink = 'chrome://os-settings/per-device-keyboard';

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
    ShortcutCustomizationAppElementBase implements
        AcceleratorsUpdatedObserverInterface, PolicyUpdatedObserverInterface,
        RouteObserver {
  static get is(): string {
    return 'shortcut-customization-app';
  }

  static get properties(): PolymerElementProperties {
    return {
      dialogShortcutTitle: {
        type: String,
        value: '',
      },

      dialogAccelerators: {
        type: Array,
        value: () => [],
      },

      dialogAction: {
        type: Number,
        value: 0,
      },

      dialogSource: {
        type: Number,
        value: 0,
      },

      showEditDialog: {
        type: Boolean,
        value: false,
      },

      showRestoreAllDialog: {
        type: Boolean,
        value: false,
      },
    };
  }

  protected showRestoreAllDialog: boolean;
  protected dialogShortcutTitle: string;
  protected dialogAccelerators: AcceleratorInfo[];
  protected dialogAction: number;
  protected dialogSource: AcceleratorSource;
  protected showEditDialog: boolean;
  protected keyboardSettingsLink: string;
  private shortcutProvider: ShortcutProviderInterface = getShortcutProvider();
  private acceleratorlookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();
  private acceleratorsUpdatedReceiver: AcceleratorsUpdatedObserverReceiver;
  private policyUpdatedReceiver: PolicyUpdatedObserverReceiver;

  override connectedCallback(): void {
    super.connectedCallback();
    if (loadTimeData.getBoolean('isJellyEnabledForShortcutCustomization')) {
      // Use dynamic color CSS and start listening to `ColorProvider` updates.
      // TODO(b/276493795): After the Jelly experiment is launched, replace
      // `cros_styles.css` with `theme/colors.css` directly in `index.html`.
      // Also add `theme/typography.css` to `index.html`.
      document.querySelector('link[href*=\'cros_styles.css\']')
          ?.setAttribute('href', 'chrome://theme/colors.css?sets=legacy,sys');
      const typographyLink = document.createElement('link');
      typographyLink.href = 'chrome://theme/typography.css';
      typographyLink.rel = 'stylesheet';
      document.head.appendChild(typographyLink);
      document.body.classList.add('jelly-enabled');
      ColorChangeUpdater.forDocument().start();
    }

    this.policyUpdatedReceiver = new PolicyUpdatedObserverReceiver(this);
    this.shortcutProvider.addPolicyObserver(
        this.policyUpdatedReceiver.$.bindNewPipeAndPassRemote());

    this.fetchAccelerators();
    this.addEventListener('show-edit-dialog', this.showDialog);
    this.addEventListener('edit-dialog-closed', this.onDialogClosed);
    this.addEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators);

    this.keyboardSettingsLink =
        loadTimeData.getBoolean('isInputDeviceSettingsSplitEnabled') ?
        newKeyboardSettingsLink :
        oldKeyboardSettingsLink;

    Router.getInstance().addObserver(this);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.policyUpdatedReceiver.$.close();
    this.acceleratorsUpdatedReceiver.$.close();
    this.removeEventListener('show-edit-dialog', this.showDialog);
    this.removeEventListener('edit-dialog-closed', this.onDialogClosed);
    this.removeEventListener(
        'request-update-accelerator', this.onRequestUpdateAccelerators);

    Router.getInstance().removeObserver(this);
  }

  private fetchAccelerators(): void {
    // Kickoff fetching accelerators by first fetching the accelerator configs.
    this.shortcutProvider.getAccelerators().then(
        ({config}) => this.onAcceleratorConfigFetched(config));

    // Fetch the hasLauncherButton value.
    this.shortcutProvider.hasLauncherButton().then(({hasLauncherButton}) => {
      this.acceleratorlookupManager.setHasLauncherButton(hasLauncherButton);
    });
  }

  private onAcceleratorConfigFetched(config: MojoAcceleratorConfig): void {
    this.acceleratorlookupManager.setAcceleratorLookup(config);
    // After fetching the config infos, fetch the layout infos next.
    this.shortcutProvider.getAcceleratorLayoutInfos().then(
        ({layoutInfos}) => this.onLayoutInfosFetched(layoutInfos));
  }

  private onLayoutInfosFetched(layoutInfos: MojoLayoutInfo[]): void {
    this.addNavigationSelectors(layoutInfos);
    this.acceleratorlookupManager.setAcceleratorLayoutLookup(layoutInfos);
    // Notify pages to update their accelerators.
    this.$.navigationPanel.notifyEvent('updateAccelerators');

    // After fetching initial accelerators, start observing for any changes.
    this.acceleratorsUpdatedReceiver =
        new AcceleratorsUpdatedObserverReceiver(this);
    this.shortcutProvider.addObserver(
        this.acceleratorsUpdatedReceiver.$.bindNewPipeAndPassRemote());
    // Navigate to the selected shortcuts if one was set from the launcher
    // search. If the url does not contain action or category info, the
    // onRouteChanged does not do anything.
    this.onRouteChanged(new URL(window.location.href));
  }

  // AcceleratorsUpdatedObserverInterface:
  onAcceleratorsUpdated(config: MojoAcceleratorConfig): void {
    this.acceleratorlookupManager.setAcceleratorLookup(config);
    // Update subsections.
    this.$.navigationPanel.notifyEvent('updateSubsections');
    // Update dialog accelerators.
    if (this.acceleratorlookupManager.isStandardAcceleratorById(
            getAcceleratorId(this.dialogSource, this.dialogAction))) {
      this.updateDialogAccelerators(this.dialogSource, this.dialogAction);
    }

    // Update the hasLauncherButton value every time accelerators are updated.
    this.shortcutProvider.hasLauncherButton().then(({hasLauncherButton}) => {
      this.acceleratorlookupManager.setHasLauncherButton(hasLauncherButton);
    });
  }

  // PolicyUpdatedObserverInterface:
  onCustomizationPolicyUpdated(): void {
    // Reload the page to apply the changes.
    window.location.reload();
  }

  private addNavigationSelectors(layoutInfos: MojoLayoutInfo[]): void {
    // A Set is used here to remove duplicates from the array of categories.
    const uniqueCategoriesInOrder =
        new Set(layoutInfos.map(layoutInfo => layoutInfo.category));
    const pages = Array.from(uniqueCategoriesInOrder).map(category => {
      const categoryNameStringId = getCategoryNameStringId(category);
      const categoryName = this.i18n(categoryNameStringId);
      return this.$.navigationPanel.createSelectorItem(
          categoryName, 'shortcuts-page', '', `category-${category}`,
          {category});
    });
    this.$.navigationPanel.addSelectors(pages);
  }

  private showDialog(e: ShowEditDialogEvent): void {
    this.dialogShortcutTitle = e.detail.description;
    this.dialogAccelerators = e.detail.accelerators;
    this.dialogAction = e.detail.action;
    this.dialogSource = e.detail.source;
    this.showEditDialog = true;
  }

  private onDialogClosed(): void {
    this.showEditDialog = false;
    this.dialogShortcutTitle = '';
    this.dialogAccelerators = [];
  }

  onRouteChanged(url: URL): void {
    const action = url.searchParams.get('action');
    const category = url.searchParams.get('category');
    if (!action || !category) {
      // This route change did not include the params that would cause the page
      // to be changed.
      return;
    }

    // Select the correct page based on the category from the URL.
    // Scrolling to the specific shortcut from the URL is handled
    // in shortcuts_page.ts.
    this.$.navigationPanel.selectPageById(`category-${category}`);
  }

  private onRequestUpdateAccelerators(e: RequestUpdateAcceleratorEvent): void {
    // Update subsections.
    this.$.navigationPanel.notifyEvent('updateSubsections');
    // Update dialog accelerators.
    if (this.acceleratorlookupManager.isStandardAcceleratorById(
            getAcceleratorId(e.detail.source, e.detail.action))) {
      this.updateDialogAccelerators(e.detail.source, e.detail.action);
    }
  }

  protected onRestoreAllDefaultClicked(): void {
    this.showRestoreAllDialog = true;
  }

  protected onCancelRestoreButtonClicked(): void {
    this.closeRestoreAllDialog();
  }

  protected onConfirmRestoreButtonClicked(): void {
    this.shortcutProvider.restoreAllDefaults().then(({result}) => {
      // TODO(jimmyxgong): Explore error state with restore all.
      if (result.result === AcceleratorConfigResult.kSuccess) {
        this.shortcutProvider.recordUserAction(UserAction.kResetAll);
        this.closeRestoreAllDialog();
      }
    });
  }

  protected closeRestoreAllDialog(): void {
    this.showRestoreAllDialog = false;
  }

  protected shouldHideRestoreAllButton(): boolean {
    return !isCustomizationAllowed();
  }

  protected updateDialogAccelerators(
      source: number|string, action: number|string): void {
    assert(this.acceleratorlookupManager.isStandardAcceleratorById(
        getAcceleratorId(source, action)));
    const updatedAccels =
        this.acceleratorlookupManager.getStandardAcceleratorInfos(
            source, action);
    this.shadowRoot!.querySelector<AcceleratorEditDialogElement>('#editDialog')!
        .updateDialogAccelerators(updatedAccels as AcceleratorInfo[]);
  }

  static get template(): HTMLTemplateElement {
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
