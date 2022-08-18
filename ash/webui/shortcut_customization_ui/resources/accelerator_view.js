// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './input_key.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AcceleratorLookupManager} from './accelerator_lookup_manager.js';
import {getTemplate} from './accelerator_view.html.js';
import {getShortcutProvider} from './mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType, Modifier, ShortcutProviderInterface} from './shortcut_types.js';

const ModifierRawKeys = [
  /*Shift=*/ 16,
  /*Alt=*/ 17,
  /*Ctrl=*/ 18,
  /*MetaLeft=*/ 91,
  /*MetaRight=*/ 92,
];

const KeyState = {
  NOT_SELECTED: 'not-selected',
  MODIFIER: 'modifier-selected',
  ALPHANUMERIC: 'alpha-numeric-selected',
};

export const ViewState = {
  VIEW: 0,
  ADD: 1,
  EDIT: 2,
};

/**
 * Returns the converted modifier flag as a readable string.
 * TODO(jimmyxgong): Localize, replace with icon, or update strings.
 * @param {number} modifier
 * @return {string}
 */
function GetModifierString(modifier) {
  switch(modifier) {
    case Modifier.SHIFT:
      return 'shift';
    case Modifier.CONTROL:
      return 'ctrl';
    case Modifier.ALT:
      return 'alt';
    case Modifier.COMMAND:
      return 'meta';
    default:
      assertNotReached();
      return '';
  }
}

/** @return {!AcceleratorInfo} */
function CreateEmptyAcceleratorInfo() {
  return /** @type {!AcceleratorInfo} */ ({
    accelerator: /* @type {!AcceleratorKeys} */(
        {modifiers: 0, key: 0, key_display: ''}),
    type: AcceleratorType.kDefault,
    state: AcceleratorState.kEnabled,
  });
}

/**
 * @fileoverview
 * 'accelerator-view' is wrapper component for an accelerator. It maintains both
 * the read-only and editable state of an accelerator.
 * TODO(jimmyxgong): Implement the edit mode.
 */
export class AcceleratorViewElement extends PolymerElement {
  static get is() {
    return 'accelerator-view';
  }

  static get properties() {
    return {
      /** @type {!AcceleratorInfo} */
      acceleratorInfo: {
        type: Object,
        value: () => {},
      },

      /** @type {!AcceleratorInfo} */
      pendingAcceleratorInfo_: {
        type: Object,
        value: () => {},
      },

      /** @private */
      acceleratorOnHold_: {
        type: String,
        value: '',
      },

      viewState: {
        type: Number,
        value: ViewState.VIEW,
        notify: true,
        observer: 'onViewStateChanged_',
      },

      /**
       * @type{!Array<string>}
       * @private
       */
      modifiers_: {
        type: Array,
        computed: 'getModifiers_(acceleratorInfo.accelerator.*)',
      },

      /** @private */
      isCapturing_: {
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
      },

      action: {
        type: Number,
        value: 0,
      },

      /** @type {!AcceleratorSource} */
      source: {
        type: Number,
        value: 0,
      },
    };
  }

  /** @override */
  constructor() {
    super();

    /** @private {!ShortcutProviderInterface} */
    this.shortcutProvider_ = getShortcutProvider();

    /** @private {!AcceleratorLookupManager} */
    this.lookupManager_ = AcceleratorLookupManager.getInstance();
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  getModifiers_() {
    const modifiers = [];
    for (const key in Modifier) {
      const modifier = Modifier[key];
      if (this.acceleratorInfo.accelerator.modifiers & modifier) {
        modifiers.push(GetModifierString(modifier));
      }
    }
    return modifiers;
  }

  /** @protected */
  onViewStateChanged_() {
    if (this.viewState !== ViewState.VIEW) {
      this.registerKeyEventListeners_();
      return;
    }
    this.unregisterKeyEventListeners_();
  }

  /** @private */
  registerKeyEventListeners_() {
    this.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.addEventListener('keyup', this.onKeyUp_.bind(this));
    this.addEventListener('focus', this.startCapture_.bind(this));
    this.addEventListener('mouseup', this.startCapture_.bind(this));
    this.addEventListener('blur', this.endCapture_.bind(this));
    this.$.container.focus();
  }

  /** @private */
  unregisterKeyEventListeners_() {
    this.removeEventListener('keydown', this.onKeyDown_.bind(this));
    this.removeEventListener('keyup', this.onKeyUp_.bind(this));
    this.removeEventListener('focus', this.startCapture_.bind(this));
    this.removeEventListener('mouseup', this.startCapture_.bind(this));
    this.removeEventListener('blur', this.endCapture_.bind(this));
  }


  /** @private */
  startCapture_() {
    if (this.isCapturing_) {
      return;
    }
    // Disable ChromeOS accelerator handler when starting input capture.
    this.pendingAcceleratorInfo_ = CreateEmptyAcceleratorInfo();
    this.isCapturing_ = true;

    this.dispatchEvent(new CustomEvent('accelerator-capturing-started', {
      bubbles: true,
      composed: true,
    }));
  }

  /** @private */
  endCapture_() {
    if (!this.isCapturing_) {
      return;
    }

    this.viewState = ViewState.VIEW;
    this.statusMessage = '';
    this.hasError = false;
    this.isCapturing_ = false;
    this.pendingAcceleratorInfo_ = CreateEmptyAcceleratorInfo();

    this.dispatchEvent(new CustomEvent('accelerator-capturing-ended', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_(e) {
    this.handleKey_(/** @type {!KeyboardEvent}*/ (e));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyUp_(e) {
    e.preventDefault();
    e.stopPropagation();
    // TODO(jimmyxgong): Check for errors e.g. accelerator conflicts.
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  handleKey_(e) {
    // While capturing, we prevent all events from bubbling, to prevent
    // shortcuts from executing and interrupting the input capture.
    e.preventDefault();
    e.stopPropagation();

    if (!this.hasValidModifiers_(e)) {
      // TODO(jimmyxgong): Fire events for error handling, e.g. Shift cannot be
      // the only modifier.
      this.pendingAcceleratorInfo_ = CreateEmptyAcceleratorInfo();
      return;
    }
    this.set('pendingAcceleratorInfo_.accelerator',
        this.keystrokeToAccelerator_(e));

    const pendingKeys = this.pendingAcceleratorInfo_.accelerator;
    // New shortcut matches the current shortcut, end capture.
    if (JSON.stringify(pendingKeys) ===
        JSON.stringify(this.acceleratorInfo.accelerator)) {
      this.endCapture_();
      return;
    }

    // Only process valid accelerators.
    if (this.isValidDefaultAccelerator_(pendingKeys)) {
      this.processPendingAccelerator_(pendingKeys);
    }
  }

  /**
   * Checks that |pendingAccelerator_| is not a pre-existing shortcut. Sets the
   * error message if there is a conflict.
   * @param {!AcceleratorKeys} pendingKeys
   * @private
   */
  processPendingAccelerator_(pendingKeys) {
    // Reset status state when processing the new accelerator.
    this.statusMessage = '';
    this.hasError = false;

    // If |acceleratorOnHold_| is not empty then the user has attempted to
    // replace a pre-existing accelerator. Check that the new accelerator
    // matches the |acceleratorOnHold_|, otherwise reset its value.
    if (this.acceleratorOnHold_ !== '') {
      if (this.acceleratorOnHold_ === JSON.stringify(pendingKeys)) {
        // User re-pressed the shortcut, send a request to replace the
        // accelerator.
        this.requestUpdateAccelerator_(pendingKeys);
        return;
      }
      this.acceleratorOnHold_ = '';
    }

    const foundId = this.lookupManager_.getAcceleratorFromKeys(
        JSON.stringify(pendingKeys));

    // Pre-existing shortcut, update the error message.
    if (foundId !== undefined) {
      // TODO(jimmyxgong): Fetch name of accelerator with real implementation.
      const uuidParams = foundId.split('-');
      const conflictSource =
          /** @type {!AcceleratorSource} */(parseInt(uuidParams[0], 10));
      const conflictAction = parseInt(uuidParams[1], 10);
      const conflictAccelName = this.lookupManager_.getAcceleratorName(
          conflictSource, conflictAction);

      // Cannot override a locked action.
      if (!this.shortcutProvider_.isMutable(conflictSource) ||
          this.lookupManager_.isAcceleratorLocked(
              conflictSource, conflictAction, pendingKeys)) {
        // TODO(jimmyxgong): i18n this string.
        this.statusMessage = 'Shortcut is used by \"' + conflictAccelName +
            '\". Press a new shortcut to replace.';
        this.hasError = true;
        return;
      }

      // TODO(jimmyxgong): i18n this string.
      this.statusMessage = 'Shortcut is used by ' + conflictAccelName +
          '. Press a new shortcut or press the same one again to use it for ' +
          'this action instead.';
      this.hasError = true;

      // Store the pending accelerator.
      this.acceleratorOnHold_ =
          JSON.stringify(this.pendingAcceleratorInfo_.accelerator);
      return;
    }

    // No conflicts, request replacement.
    this.requestUpdateAccelerator_(pendingKeys);
  }

  /**
   * Converts a keystroke event to an Accelerator Object.
   * @param {!KeyboardEvent} e
   * @return {!AcceleratorKeys} The keystroke as an Acccelerator object.
   * @private
   */
  keystrokeToAccelerator_(e) {
    const output = /** @type {AcceleratorKeys} */({
        modifiers: 0, key: 0, key_display: ''});
    if (e.metaKey) {
      output.modifiers = output.modifiers | Modifier.COMMAND;
    }
    if (e.ctrlKey) {
      output.modifiers = output.modifiers | Modifier.CONTROL;
    }
    if (e.altKey) {
      output.modifiers = output.modifiers | Modifier.ALT;
    }
    // Shift key isn't registered as a modifier unless a non-modifer key is
    // pressed in conjunction with the keystroke.
    if (e.key == 'Shift' || e.shiftKey) {
      output.modifiers = output.modifiers | Modifier.SHIFT;
    }

    // Only add non-modifier keys as the pending key.
    if (!this.isModifierKey_(e)) {
      output.key_display = e.key;
      output.key = e.keyCode;
    }

    return output;
  }

  /**
   * @param {!KeyboardEvent} e
   * @return {boolean}
   * @private
   */
  isModifierKey_(e) {
    return ModifierRawKeys.includes(e.keyCode);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @protected
   */
  getCtrlState_() {
    return this.getModifierState_(Modifier.CONTROL);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @protected
   */
  getAltState_() {
    return this.getModifierState_(Modifier.ALT);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @protected
   */
  getShiftState_() {
    return this.getModifierState_(Modifier.SHIFT);
  }

  /**
   * @return {string} The specified CSS state of the modifier key element.
   * @protected
   */
  getSearchState_() {
    return this.getModifierState_(Modifier.COMMAND);
  }

  /**
   * @param {number} modifier
   * @return {string} The specified CSS state of the modifier key element.
   * @private
   */
  getModifierState_(modifier) {
    if (this.pendingAcceleratorInfo_.accelerator.modifiers & modifier) {
      return KeyState.MODIFIER;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * @return {string} The specified CSS state of the pending key element.
   * @protected
   */
  getPendingKeyState_() {
    if (this.pendingAcceleratorInfo_.accelerator.key_display != '') {
      return KeyState.ALPHANUMERIC;
    }
    return KeyState.NOT_SELECTED;
  }

  /**
   * @return {string} The specified key to display.
   * @protected
   */
  getPendingKey_() {
    if (this.pendingAcceleratorInfo_.accelerator.key_display != '') {
      return this.pendingAcceleratorInfo_.accelerator.key_display.toLowerCase();
    }
    // TODO(jimmyxgong): Reset to a localized default empty state.
    return 'key';
  }

  /**
   * Returns true if the event has valid modifiers.
   * @param {!KeyboardEvent} e The keyboard event to consider.
   * @return {boolean} True if the event is valid.
   * @private
   */
  hasValidModifiers_(e) {
    // Although Shift is a modifier, it cannot be a standalone modifier for a
    // shortcut.
    return e.ctrlKey || e.altKey || e.metaKey;
  }

  /**
   * @param {AcceleratorKeys} keys
   * @return {boolean}
   * @private
   */
  isValidDefaultAccelerator_(keys) {
    // A valid default accelerator is on that has modifier(s) and a key.
    return keys.modifiers > 0 && keys.key_display !== '';
  }

  /**
   * @return {boolean}
   * @private
   */
  showEditView_() {
    return this.viewState !== ViewState.VIEW;
  }

  /**
   * @param {!AcceleratorKeys} newKeys
   * @private
   */
  requestUpdateAccelerator_(newKeys) {
    if (this.viewState === ViewState.EDIT) {
      this.shortcutProvider_
          .replaceAccelerator(
              this.source, this.action, this.acceleratorInfo.accelerator,
              newKeys)
          .then((result) => {
            // TODO(jimmyxgong): Handle other error cases.
            if (result === AcceleratorConfigResult.kSuccess) {
              this.lookupManager_.replaceAccelerator(
                  this.source, this.action, this.acceleratorInfo.accelerator,
                  newKeys);
              this.fireUpdateEvent_();
            }
          });
    }

    if (this.viewState === ViewState.ADD) {
      this.shortcutProvider_
          .addUserAccelerator(this.source, this.action, newKeys)
          .then((result) => {
            // TODO(jimmyxgong): Handle other error cases.
            if (result === AcceleratorConfigResult.kSuccess) {
              this.lookupManager_.addAccelerator(
                  this.source, this.action, newKeys);
              this.fireUpdateEvent_();
            }
          });
    }
  }

  /** @private */
  fireUpdateEvent_() {
    this.dispatchEvent(new CustomEvent('request-update-accelerator', {
      bubbles: true,
      composed: true,
      detail: {source: this.source, action: this.action},
    }));

    // Always end input capturing if an update event was fired.
    this.endCapture_();
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(AcceleratorViewElement.is, AcceleratorViewElement);
