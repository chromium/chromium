// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';

import {KeyEvent} from 'chrome://resources/ash/common/shortcut_input_ui/input_device_settings.mojom-webui.js';
import {ShortcutInputElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorResultData, Subactions, UserAction} from '../mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';
import {ShortcutInputProviderInterface} from '../mojom-webui/shortcut_input_provider.mojom-webui.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_view.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {getShortcutInputProvider} from './shortcut_input_mojo_interface_provider.js';
import {Accelerator, AcceleratorConfigResult, AcceleratorSource, AcceleratorState, EditAction, Modifier, ShortcutProviderInterface, StandardAcceleratorInfo} from './shortcut_types.js';
import {areAcceleratorsEqual, canBypassErrorWithRetry, getAccelerator, getKeyDisplay, getModifiersForAcceleratorInfo, isCustomizationAllowed, isStandardAcceleratorInfo, isValidDefaultAccelerator, keyEventToAccelerator, LWIN_KEY, META_KEY, resetKeyEvent} from './shortcut_utils.js';

export interface AcceleratorViewElement {
  $: {
    container: HTMLDivElement,
    shortcutInput: ShortcutInputElement,
  };
}

export enum ViewState {
  VIEW,
  ADD,
  EDIT,
}

// This delay should match the animation timing in `shortcut_input_key.html`.
// Matching the delay allows the user to see the full animation before
// requesting a change to the backend.
const kAnimationTimeoutMs: number = 300;

const kEscapeKey: number = 27;  // Keycode for VKEY_ESCAPE

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
const AcceleratorViewElementBase = I18nMixin(PolymerElement);

export class AcceleratorViewElement extends AcceleratorViewElementBase {
  static get is(): string {
    return 'accelerator-view';
  }

  static get properties(): PolymerElementProperties {
    return {
      acceleratorInfo: {
        type: Object,
      },

      pendingKeyEvent: {
        type: Object,
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
        observer: AcceleratorViewElement.prototype.onViewStateChanged,
      },

      modifiers: {
        type: Array,
        computed: 'getModifiers(acceleratorInfo.accelerator.*)',
      },

      isCapturing: {
        type: Boolean,
        value: false,
      },

      statusMessage: {
        type: String,
        notify: true,
      },

      /** Informs parent components that an error has occurred. */
      hasError: {
        type: Boolean,
        value: false,
        notify: true,
        observer: AcceleratorViewElement.prototype.onErrorUpdated,
      },

      // Keeps track if there was ever an error when interacting with this
      // accelerator.
      recordedError: {
        type: Boolean,
        value: false,
        notify: true,
      },

      action: {
        type: Number,
        value: 0,
      },

      source: {
        type: Number,
        value: 0,
      },

      sourceIsLocked: {
        type: Boolean,
        value: false,
      },

      /**
       * Conditionally show the edit-icon-container in `accelerator-view`, true
       * for `accelerator-row`, false for `accelerator-edit-view`.
       */
      showEditIcon: {
        type: Boolean,
        value: false,
      },

      /** Only show the edit button in the first row. */
      isFirstAccelerator: {
        type: Boolean,
      },

      isDisabled: {
        type: Boolean,
        computed: 'computeIsDisabled(acceleratorInfo.*)',
        reflectToAttribute: true,
      },

      /** Whether to show a launcher icon or search icon for meta key. */
      hasLauncherButton: Boolean,
    };
  }

  acceleratorInfo: StandardAcceleratorInfo;
  viewState: ViewState;
  statusMessage: string;
  hasError: boolean;
  recordedError: boolean;
  action: number;
  source: AcceleratorSource;
  sourceIsLocked: boolean;
  showEditIcon: boolean;
  categoryIsLocked: boolean;
  isFirstAccelerator: boolean;
  isDisabled: boolean;
  hasLauncherButton: boolean;
  pendingKeyEvent: KeyEvent|null = null;
  shortcutInput: ShortcutInputElement|null;
  protected isCapturing: boolean;
  protected lastAccelerator: Accelerator;
  protected lastResult: AcceleratorConfigResult;
  protected lastPendingKeyEvent: KeyEvent|null = null;
  private shortcutProvider: ShortcutProviderInterface = getShortcutProvider();
  private lookupManager: AcceleratorLookupManager =
      AcceleratorLookupManager.getInstance();
  private eventTracker: EventTracker = new EventTracker();
  private editAction: EditAction = EditAction.NONE;

  override connectedCallback(): void {
    super.connectedCallback();

    this.categoryIsLocked = this.lookupManager.isCategoryLocked(
        this.lookupManager.getAcceleratorCategory(this.source, this.action));
    this.hasLauncherButton = this.lookupManager.getHasLauncherButton();
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  getShortcutInputProvider(): ShortcutInputProviderInterface {
    return getShortcutInputProvider();
  }

  private getModifiers(): string[] {
    return getModifiersForAcceleratorInfo(this.acceleratorInfo);
  }

  protected onViewStateChanged(): void {
    if (this.viewState !== ViewState.VIEW) {
      this.registerKeyEventListeners();
      return;
    }
    this.unregisterKeyEventListeners();
  }

  protected onShortcutInputDomChange(): void {
    // `shortcutInput` will always be restamped when `viewState` is Edit.
    // Start observing for input events the moment `shortcutInput` is available.
    this.shortcutInput =
        this.shadowRoot!.querySelector<ShortcutInputElement>('#shortcutInput');
    if (this.shortcutInput) {
      this.shortcutInput.startObserving();
    }
  }

  private registerKeyEventListeners(): void {
    this.eventTracker.add(
        this, 'shortcut-input-capture-state',
        (e: CustomEvent) => this.onShortcutInputCaptureStateUpdate(e));
    this.eventTracker.add(
        this, 'shortcut-input-event',
        (e: CustomEvent) => this.handleKeyDown(e));
  }

  private unregisterKeyEventListeners(): void {
    this.eventTracker.removeAll();
  }

  private startCapture(): void {
    if (this.isCapturing) {
      return;
    }

    this.pendingKeyEvent = resetKeyEvent();

    // Announce hint message when focus and start capture.
    this.makeA11yAnnouncement(this.i18n('editViewStatusMessage'));
  }

  async endCapture(shouldDelay: boolean): Promise<void> {
    this.editAction = EditAction.NONE;

    if (this.shortcutInput) {
      this.shortcutInput.stopObserving();
    }

    this.dispatchEvent(new CustomEvent('accelerator-capturing-ended', {
      bubbles: true,
      composed: true,
    }));

    // Delay if an update event is fired.
    if (shouldDelay) {
      await new Promise(resolve => setTimeout(resolve, kAnimationTimeoutMs));
      // Dispatch event to update subsections and dialog accelerators.
      this.dispatchEvent(new CustomEvent('request-update-accelerator', {
        bubbles: true,
        composed: true,
        detail: {source: this.source, action: this.action},
      }));
    }

    this.viewState = ViewState.VIEW;
    // Should always set `hasError` before `statusMessage` since `statusMessage`
    // is dependent on `hasError`'s state.
    this.hasError = false;
    this.statusMessage = '';
    this.pendingKeyEvent = resetKeyEvent();
  }

  private onShortcutInputCaptureStateUpdate(e: CustomEvent): void {
    if (this.isCapturing === e.detail.capturing) {
      // Ignore repeated events.
      return;
    }

    this.isCapturing = e.detail.capturing;
    if (this.isCapturing) {
      this.dispatchEvent(new CustomEvent('accelerator-capturing-started', {
        bubbles: true,
        composed: true,
      }));
      this.startCapture();
    }
  }

  private handleKeyDown(e: CustomEvent): void {
    const rewrittenKeyEvent = e.detail.keyEvent;
    const pendingAccelerator = keyEventToAccelerator(rewrittenKeyEvent);
    if (this.hasError) {
      // If an error occurred, check if the pending accelerator matches the
      // last. If they match and a retry on the same accelerator
      // cannot bypass the error, exit early to prevent flickering error
      // messages.
      if (areAcceleratorsEqual(pendingAccelerator, this.lastAccelerator) &&
          !canBypassErrorWithRetry(this.lastResult)) {
        return;
      }
      // Reset status state when pressing a new key.
      this.statusMessage = '';
      this.hasError = false;
    }

    this.lastAccelerator = {...pendingAccelerator};
    // Alt + Esc will exit input handling immediately.
    if (pendingAccelerator.modifiers === Modifier.ALT &&
        pendingAccelerator.keyCode === kEscapeKey) {
      this.endCapture(/*shouldDelay=*/ false);
      return;
    }

    // Only process valid accelerators.
    if (isValidDefaultAccelerator(pendingAccelerator)) {
      // Store the pending key event.
      this.lastPendingKeyEvent = rewrittenKeyEvent;
      this.processPendingAccelerator(pendingAccelerator);
    }
  }

  private async processPendingAccelerator(pendingAccelerator: Accelerator):
      Promise<void> {
    // Dispatch an event indicating that accelerator update is in progress.
    this.dispatchEvent(new CustomEvent('accelerator-update-in-progress', {
      bubbles: true,
      composed: true,
    }));
    // Reset status state when processing the new accelerator.
    this.statusMessage = '';
    this.hasError = false;

    let result: {result: AcceleratorResultData};
    assert(this.viewState !== ViewState.VIEW);

    // If the accelerator is disabled, we should only add the new accelerator.
    const isDisabledAccelerator =
        this.acceleratorInfo.state === AcceleratorState.kDisabledByUser;

    if (this.viewState === ViewState.ADD || isDisabledAccelerator) {
      this.editAction = EditAction.ADD;
      result = await this.shortcutProvider.addAccelerator(
          this.source, this.action, pendingAccelerator);
    }

    if (this.viewState === ViewState.EDIT && !isDisabledAccelerator) {
      this.editAction = EditAction.EDIT;
      const originalAccelerator: Accelerator|undefined =
          this.acceleratorInfo.layoutProperties.standardAccelerator
              ?.originalAccelerator;
      const acceleratorToEdit =
          originalAccelerator || getAccelerator(this.acceleratorInfo);
      result = await this.shortcutProvider.replaceAccelerator(
          this.source, this.action, acceleratorToEdit, pendingAccelerator);
    }
    this.handleAcceleratorResultData(result!.result);
  }

  private handleAcceleratorResultData(result: AcceleratorResultData): void {
    this.lastResult = result.result;
    switch (result.result) {
      // Shift is the only modifier.
      case AcceleratorConfigResult.kShiftOnlyNotAllowed: {
        this.statusMessage = this.i18n('shiftOnlyNotAllowedStatusMessage');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // No modifiers is pressed before primary key.
      case AcceleratorConfigResult.kMissingModifier: {
        // This is a backup check, since only valid accelerators are processed
        // and a valid accelerator will have modifier(s) and a key or is
        // function key.
        this.statusMessage = this.i18n('missingModifierStatusMessage');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // Top row key used as activation keys(no search key pressed).
      case AcceleratorConfigResult.kKeyNotAllowed: {
        this.statusMessage = this.i18n('keyNotAllowedStatusMessage');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // Search with function keys are not allowed.
      case AcceleratorConfigResult.kSearchWithFunctionKeyNotAllowed: {
        this.statusMessage =
            this.i18n('searchWithFunctionKeyNotAllowedStatusMessage');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // Conflict with a locked accelerator.
      case AcceleratorConfigResult.kConflict:
      case AcceleratorConfigResult.kActionLocked: {
        this.statusMessage = this.i18n(
            'lockedShortcutStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // Conflict with an editable shortcut.
      case AcceleratorConfigResult.kConflictCanOverride: {
        this.statusMessage = this.i18n(
            'shortcutWithConflictStatusMessage',
            mojoString16ToString(result.shortcutName as String16));
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      // Limit to only 5 accelerators allowed.
      case AcceleratorConfigResult.kMaximumAcceleratorsReached: {
        this.statusMessage = this.i18n('maxAcceleratorsReachedHint');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      case AcceleratorConfigResult.kNonSearchAcceleratorWarning: {
        // TODO(jimmyxgong): Add the "Learn More" link when available.
        this.statusMessage = this.i18n('warningSearchNotIncluded');
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      case AcceleratorConfigResult.kReservedKeyNotAllowed: {
        this.statusMessage = this.i18n(
            'reservedKeyNotAllowedStatusMessage',
            this.lastPendingKeyEvent!.keyDisplay);
        this.hasError = true;
        this.makeA11yAnnouncement(this.statusMessage);
        return;
      }
      case AcceleratorConfigResult.kSuccess: {
        this.fireEditCompletedActionEvent(this.editAction);
        getShortcutProvider().recordAddOrEditSubactions(
            this.viewState === ViewState.ADD,
            this.recordedError ? Subactions.kErrorSuccess :
                                 Subactions.kNoErrorSuccess);
        getShortcutProvider().recordUserAction(
            UserAction.kSuccessfulModification);
        const message = (this.viewState == ViewState.ADD) ?
            this.i18n('shortcutAdded') :
            this.i18n('shortcutEdited');
        this.makeA11yAnnouncement(message);
        this.fireUpdateEvent();
        return;
      }
    }
    assertNotReached();
  }


  private makeA11yAnnouncement(message: string): void {
    const announcer = getAnnouncerInstance(this.$.container);
    // Remove "role = alert" to avoid chromevox announcing "alert" before
    // message.
    strictQuery('#messages', announcer.shadowRoot, HTMLDivElement)
        .removeAttribute('role');
    // Announce the messages.
    announcer.announce(message);
  }

  private showEditView(): boolean {
    return this.viewState !== ViewState.VIEW;
  }

  private fireUpdateEvent(): void {
    if (this.acceleratorInfo.state === AcceleratorState.kDisabledByUser &&
        isStandardAcceleratorInfo(this.acceleratorInfo)) {
      this.dispatchEvent(new CustomEvent('default-conflict-resolved', {
        bubbles: true,
        composed: true,
        detail: {
          stringifiedAccelerator:
              JSON.stringify(getAccelerator(this.acceleratorInfo)),
        },
      }));
    }

    // Always end input capturing if an update event was fired.
    this.endCapture(/*should_delay=*/ true);
  }

  private fireEditCompletedActionEvent(editAction: EditAction): void {
    this.dispatchEvent(new CustomEvent('edit-action-completed', {
      bubbles: true,
      composed: true,
      detail: {
        editAction: editAction,
      },
    }));
  }

  private shouldShowLockIcon(): boolean {
    // Do not show lock icon in each row if customization is disabled or its
    // category is locked.
    if (!isCustomizationAllowed() || this.categoryIsLocked) {
      return false;
    }
    // Show lock icon if accelerator is locked.
    return (this.acceleratorInfo && this.acceleratorInfo.locked) ||
        this.sourceIsLocked;
  }

  private shouldShowEditIcon(): boolean {
    // Do not show edit icon in each row if customization is disabled, the row
    // is displayed in edit-dialog(!showEditIcon) or category is locked.
    if (!isCustomizationAllowed() || !this.showEditIcon ||
        this.categoryIsLocked) {
      return false;
    }
    // Show edit icon if accelerator is not locked.
    return !(this.acceleratorInfo && this.acceleratorInfo.locked) &&
        !this.sourceIsLocked && this.isFirstAccelerator;
  }

  private onEditIconClicked(): void {
    this.dispatchEvent(
        new CustomEvent('edit-icon-clicked', {bubbles: true, composed: true}));
  }

  private getAriaLabel(): string {
    // Clear aria-label during editing to avoid unnecessary chromevox
    // announcements.
    if (this.viewState !== ViewState.VIEW) {
      return '';
    }
    let keyOrIcon =
        this.acceleratorInfo.layoutProperties.standardAccelerator.keyDisplay;
    const metaKeyAriaLabel = this.lookupManager.getHasLauncherButton() ?
        this.i18n('iconLabelOpenLauncher') :
        this.i18n('iconLabelOpenSearch');
    // LWIN_KEY is not a modifier, but it is displayed as a meta icon.
    keyOrIcon = keyOrIcon === LWIN_KEY ? metaKeyAriaLabel : keyOrIcon;
    const modifiers =
        getModifiersForAcceleratorInfo(this.acceleratorInfo)
            .map(
                // Update modifiers if it includes META_KEY.
                modifier =>
                    modifier === META_KEY ? metaKeyAriaLabel : modifier);

    return [...modifiers, getKeyDisplay(keyOrIcon)].join(' ');
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private computeIsDisabled(): boolean {
    return this.acceleratorInfo.state === AcceleratorState.kDisabledByUser ||
        this.acceleratorInfo.state === AcceleratorState.kDisabledByConflict;
  }

  private onErrorUpdated(): void {
    // `recordedError` will only update if it was previously false and
    // an error has been detected.
    if (!this.recordedError && this.hasError) {
      this.recordedError = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'accelerator-view': AcceleratorViewElement;
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
