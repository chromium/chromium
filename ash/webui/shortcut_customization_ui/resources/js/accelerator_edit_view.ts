// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../common/icons.html.js';
import '../css/shortcut_customization_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icons.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Subactions, UserAction} from '../mojom-webui/shortcut_customization.mojom-webui.js';

import {getTemplate} from './accelerator_edit_view.html.js';
import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from './accelerator_view.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorKeyState, AcceleratorSource, AcceleratorState, AcceleratorType, EditAction, MetaKey, ShortcutProviderInterface, StandardAcceleratorInfo} from './shortcut_types.js';
import {getAccelerator, getAriaLabelForStandardAcceleratorInfo} from './shortcut_utils.js';

export type RequestUpdateAcceleratorEvent =
    CustomEvent<{action: number, source: AcceleratorSource}>;

declare global {
  interface HTMLElementEventMap {
    'request-update-accelerator': RequestUpdateAcceleratorEvent;
  }
}

const accelerator: Accelerator = {
  modifiers: 0,
  keyCode: 0,
  keyState: AcceleratorKeyState.PRESSED,
};

const standardAcceleratorInfoState: StandardAcceleratorInfo = {
  acceleratorLocked: false,
  locked: false,
  state: AcceleratorState.kEnabled,
  type: AcceleratorType.kDefault,
  layoutProperties: {
    standardAccelerator: {
      accelerator,
      keyDisplay: '',
    },
  },
};

/**
 * @fileoverview
 * 'accelerator-edit-view' is a wrapper component for one accelerator. It is
 * responsible for displaying the edit/remove buttons to an accelerator and also
 * displaying context or errors strings for an accelerator.
 */
const AcceleratorEditViewElementBase = I18nMixin(PolymerElement);

export class AcceleratorEditViewElement extends AcceleratorEditViewElementBase {
  static get is(): string {
    return 'accelerator-edit-view';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      acceleratorInfo: {
        type: Object,
        value: standardAcceleratorInfoState,
      },

      isEditView: {
        type: Boolean,
        computed: 'showEditView(viewState)',
        reflectToAttribute: true,
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
      },

      statusMessage: {
        type: String,
        value: '',
        observer: AcceleratorEditViewElement.prototype.onStatusMessageChanged,
      },

      hasError: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // If search is not included in a key-combination, hasWarning is set to
      // true. The visual style will be distinct from other error cases.
      hasWarning: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      // Keeps track if there was ever an error when interacting with this
      // accelerator.
      recordedError: {
        type: Boolean,
        value: false,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
      },
    };
  }

  acceleratorInfo: StandardAcceleratorInfo;
  isEditView: boolean;
  viewState: number;
  hasError: boolean;
  hasWarning: boolean;
  recordedError: boolean;
  action: number;
  source: AcceleratorSource;
  restoreDefaultHasError: boolean;
  protected statusMessage: string;
  protected cancelButtonClicked = false;
  private shortcutProvider: ShortcutProviderInterface;
  private lookupManager: AcceleratorLookupManager;

  constructor() {
    super();

    this.shortcutProvider = getShortcutProvider();

    this.lookupManager = AcceleratorLookupManager.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();
    this.addEventListener('blur', this.onBlur);
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.removeEventListener('blur', this.onBlur);
  }

  protected async onStatusMessageChanged(): Promise<void> {
    if (this.statusMessage === '') {
      if (this.acceleratorInfo.state === AcceleratorState.kDisabledByUser &&
          this.viewState !== ViewState.EDIT) {
        this.hasError = true;
        const configResult = await this.shortcutProvider.getConflictAccelerator(
            this.source, this.action, getAccelerator(this.acceleratorInfo));
        if (configResult.result.result === AcceleratorConfigResult.kConflict) {
          this.statusMessage = this.i18n(
              'restoreDefaultConflictMessage',
              mojoString16ToString(
                  configResult.result.shortcutName as String16));
        }
        return;
      } else {
        this.statusMessage = this.i18n('editViewStatusMessage');
      }
    }
    this.hasWarning = this.statusMessage ===
        this.i18n('warningSearchNotIncluded', this.getMetaKeyDisplay());
  }

  protected onEditButtonClicked(): void {
    // Reset the error messages upon clicking the edit button.
    this.viewState = ViewState.EDIT;
    this.statusMessage = '';
    this.hasError = false;
    getShortcutProvider().recordUserAction(UserAction.kStartReplaceAccelerator);
  }

  protected async onDeleteButtonClicked(): Promise<void> {
    const accelerator = getAccelerator(this.acceleratorInfo);
    // Do not attempt to remove an already disabled accelerator.
    if (this.acceleratorInfo.state === AcceleratorState.kDisabledByUser) {
      // Clicking the delete button on a disabled accelerator is a no-opt, but
      // should be marked as though the conflict has been resolved.
      this.dispatchEvent(new CustomEvent('default-conflict-resolved', {
        bubbles: true,
        composed: true,
        detail: {stringifiedAccelerator: JSON.stringify(accelerator)},
      }));

      // Re-trigger the top-level update to re-fetch the dialog accelerators.
      this.dispatchEvent(new CustomEvent('request-update-accelerator', {
        bubbles: true,
        composed: true,
        detail: {source: this.source, action: this.action},
      }));
      return;
    }

    // Check if the accelerator is an alias, if so use the original accelerator.
    const originalAccelerator: Accelerator|undefined =
        this.acceleratorInfo.layoutProperties.standardAccelerator
            ?.originalAccelerator;
    const configResult = await this.shortcutProvider.removeAccelerator(
        this.source, this.action, originalAccelerator || accelerator);

    if (configResult.result.result === AcceleratorConfigResult.kSuccess) {
      this.dispatchEvent(new CustomEvent('request-update-accelerator', {
        bubbles: true,
        composed: true,
        detail: {source: this.source, action: this.action},
      }));

      this.dispatchEvent(new CustomEvent('edit-action-completed', {
        bubbles: true,
        composed: true,
        detail: {editAction: EditAction.REMOVE},
      }));

      getShortcutProvider().recordUserAction(UserAction.kRemoveAccelerator);
    }
  }

  protected onCancelButtonClicked(): void {
    this.shortcutProvider.recordAddOrEditSubactions(
        this.viewState === ViewState.ADD,
        this.recordedError ? Subactions.kErrorCancel :
                             Subactions.kNoErrorCancel);
    this.cancelButtonClicked = true;
    this.endCapture();
  }

  protected onBlur(): void {
    // Prevent clicking cancel button triggering blur event.
    if (this.cancelButtonClicked) {
      this.cancelButtonClicked = false;
      return;
    }
    this.endCapture();
  }

  protected showEditView(): boolean {
    return this.viewState !== ViewState.VIEW;
  }

  protected showStatusMessage(): boolean {
    return this.showEditView() ||
        this.acceleratorInfo.state === AcceleratorState.kDisabledByUser;
  }

  private endCapture(): void {
    const viewElement = strictQuery(
        'accelerator-view', this.shadowRoot, AcceleratorViewElement);
    viewElement.endCapture(/*should_delay=*/ false);
  }

  private getMetaKeyDisplay(): string {
    const metaKey = this.lookupManager.getMetaKeyToDisplay();
    switch (metaKey) {
      case MetaKey.kLauncherRefresh:
        // TODO(b/338134189): Replace it with updated icon when finalized.
        return this.i18n('iconLabelOpenLauncher');
      case MetaKey.kSearch:
        return this.i18n('iconLabelOpenSearch');
      case MetaKey.kLauncher:
      default:
        return this.i18n('iconLabelOpenLauncher');
    }
  }

  getStatusMessageForTesting(): string {
    return this.statusMessage;
  }

  private getEditAriaLabel(): string {
    return this.i18n(
        'editButtonForAction',
        getAriaLabelForStandardAcceleratorInfo(this.acceleratorInfo));
  }

  private getDeleteAriaLabel(): string {
    return this.i18n(
        'deleteButtonForAction',
        getAriaLabelForStandardAcceleratorInfo(this.acceleratorInfo));
  }

  protected getLearnMoreUrl(): string {
    return this.i18n('shortcutCustomizationLearnMoreUrl');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-edit-view': AcceleratorEditViewElement;
  }
}

customElements.define(
    AcceleratorEditViewElement.is, AcceleratorEditViewElement);
