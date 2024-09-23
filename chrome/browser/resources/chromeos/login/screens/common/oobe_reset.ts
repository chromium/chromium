// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design reset screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/buttons/oobe_text_button.js';

import {getInstance as getAnnouncerInstance} from '//resources/ash/common/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrCheckboxElement} from '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './oobe_reset.html.js';

/**
 * UI state for the dialog.
 */
enum ResetScreenUiState {
  RESTART_REQUIRED = 'restart-required',
  REVERT_PROMISE = 'revert-promise',
  // POWERWASH_PROPOSAL supports 2 ui-states: with or without rollback
  POWERWASH_PROPOSAL = 'powerwash-proposal',
  ERROR = 'error',
}

/**
 * The order should be in sync with the ResetScreen::State enum.
 */
const ResetScreenUiStateMapping = [
  ResetScreenUiState.RESTART_REQUIRED,
  ResetScreenUiState.REVERT_PROMISE,
  ResetScreenUiState.POWERWASH_PROPOSAL,
  ResetScreenUiState.ERROR,
];

// When the screen is in the powerwash proposal state, it depends on the mode
enum PowerwashMode {
  'POWERWASH_WITH_ROLLBACK' = 0,
  'POWERWASH_ONLY' = 1,
}

interface DialogRessources {
  subtitleText: string;
  dialogTitle: string;
  dialogContent: string;
  buttonTextKey: string;
}

// Powerwash mode details. Used by the UI for the two different modes
const POWERWASH_MODE_DETAILS: Map<number, DialogRessources> = new Map([
  [
    PowerwashMode.POWERWASH_WITH_ROLLBACK,
    {
      subtitleText: 'resetPowerwashRollbackWarningDetails',
      dialogTitle: 'confirmRollbackTitle',
      dialogContent: 'confirmRollbackMessage',
      buttonTextKey: 'resetButtonPowerwashAndRollback',
    },
  ],
  [
    PowerwashMode.POWERWASH_ONLY,
    {
      subtitleText: 'resetPowerwashWarningDetails',
      dialogTitle: 'confirmPowerwashTitle',
      dialogContent: 'confirmPowerwashMessage',
      buttonTextKey: 'resetButtonPowerwash',
    },
  ],
]);

const ResetScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));


export class OobeReset extends ResetScreenElementBase {
  static get is() {
    return 'oobe-reset-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /** Whether rollback is available */
      isRollbackAvailable_: {
        type: Boolean,
        value: false,
        observer: 'updatePowerwashModeBasedOnRollbackOptions',
      },

      /**
       * Whether the rollback option was chosen by the user.
       */
      isRollbackRequested_: {
        type: Boolean,
        value: false,
        observer: 'updatePowerwashModeBasedOnRollbackOptions',
      },

      /**
       * Whether to show the TPM firmware update checkbox.
       */
      tpmUpdateAvailable_: {
        type: Boolean,
      },

      /**
       * If the checkbox to request a TPM firmware update is checked.
       */
      tpmUpdateChecked_: {
        type: Boolean,
      },

      /**
       * If the checkbox to request a TPM firmware update is editable.
       */
      tpmUpdateEditable_: {
        type: Boolean,
      },

      /**
       * The current TPM update mode.
       */
      tpmUpdateMode_: {
        type: String,
      },

      // Title to be shown on the confirmation dialog.
      confirmationDialogTitle_: {
        type: String,
        computed: 'getConfirmationDialogTitle(locale, powerwashMode_)',
      },

      // Content to be shown on the confirmation dialog.
      confirmationDialogText_: {
        type: String,
        computed: 'getConfirmationDialogText(locale, powerwashMode_)',
      },

      // The subtitle to be shown while the screen is in POWERWASH_PROPOSAL
      powerwashStateSubtitle_: {
        type: String,
        computed: 'getPowerwashStateSubtitle(locale, powerwashMode_)',
      },

      // The text shown on the powerwash button. (depends on powerwash mode)
      powerwashButtonTextKey_: {
        type: String,
        computed: 'getPowerwashButtonTextKey(locale, powerwashMode_)',
      },

      // Whether the powerwash button is disabled.
      powerwashButtonDisabled_: {
        type: Boolean,
        computed: 'isPowerwashDisabled(powerwashMode_, tpmUpdateChecked_)',
      },

      // The chosen powerwash mode
      powerwashMode_: {
        type: Number,
        value: PowerwashMode.POWERWASH_ONLY,
      },

      inRevertState_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers(): string[] {
    return ['onScreenStateChanged(uiStep)'];
  }

  private isRollbackAvailable_: boolean;
  private isRollbackRequested_: boolean;
  private tpmUpdateAvailable_: boolean;
  private tpmUpdateChecked_: boolean;
  private tpmUpdateEditable_: boolean;
  private tpmUpdateMode_: string;
  private confirmationDialogTitle_: string;
  private confirmationDialogText_: string;
  private powerwashStateSubtitle_: string;
  private powerwashButtonTextKey_: string;
  private powerwashButtonDisabled_: boolean;
  private powerwashMode_: number;
  private inRevertState_: boolean;

  constructor() {
    super();

    this.isRollbackAvailable_ = false;
    this.isRollbackRequested_ = false;
    this.tpmUpdateAvailable_ = false;
    this.tpmUpdateChecked_ = false;
    this.tpmUpdateEditable_ = true;
  }

  /** Overridden from LoginScreenBehavior. */
  override get EXTERNAL_API(): string[] {
    return [
      'setIsRollbackAvailable',
      'setIsRollbackRequested',
      'setIsTpmFirmwareUpdateAvailable',
      'setIsTpmFirmwareUpdateChecked',
      'setIsTpmFirmwareUpdateEditable',
      'setTpmFirmwareUpdateMode',
      'setShouldShowConfirmationDialog',
      'setScreenState',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): ResetScreenUiState {
    return ResetScreenUiState.RESTART_REQUIRED;
  }

  override get UI_STEPS() {
    return ResetScreenUiState;
  }

  override ready(): void {
    super.ready();
    // Set initial states.
    this.reset();
    this.setShouldShowConfirmationDialog(false);
    this.initializeLoginScreen('ResetScreen');
  }

  reset(): void {
    this.setUIStep(ResetScreenUiState.RESTART_REQUIRED);
    this.powerwashMode_ = PowerwashMode.POWERWASH_ONLY;
    this.isRollbackAvailable_ = false;
    this.isRollbackRequested_ = false;
    this.tpmUpdateAvailable_ = false;
    this.tpmUpdateChecked_ = false;
    this.tpmUpdateEditable_ = true;
  }

  /* ---------- EXTERNAL API BEGIN ---------- */
  setIsRollbackAvailable(rollbackAvailable: boolean): void {
    this.isRollbackAvailable_ = rollbackAvailable;
  }

  setIsRollbackRequested(rollbackRequested: boolean): void {
    this.isRollbackRequested_ = rollbackRequested;
  }

  setIsTpmFirmwareUpdateAvailable(value: boolean): void {
    this.tpmUpdateAvailable_ = value;
  }

  setIsTpmFirmwareUpdateChecked(value: boolean): void {
    this.tpmUpdateChecked_ = value;
  }

  setIsTpmFirmwareUpdateEditable(value: boolean): void {
    this.tpmUpdateEditable_ = value;
  }

  setTpmFirmwareUpdateMode(value: string): void {
    this.tpmUpdateMode_ = value;
  }


  setShouldShowConfirmationDialog(shouldShow: boolean) {
    const confirmationDialog =
        this.shadowRoot!.querySelector<OobeModalDialog>('#confirmationDialog')!;
    if (shouldShow) {
      confirmationDialog.showDialog();
    } else {
      confirmationDialog.hideDialog();
    }
  }

  setScreenState(state: number): void {
    this.setUIStep(ResetScreenUiStateMapping[state]);
  }
  /* ---------- EXTERNAL API END ---------- */

  /**
   *  When rollback is available and requested, the powerwash mode changes
   *  to POWERWASH_WITH_ROLLBACK.
   */
  private updatePowerwashModeBasedOnRollbackOptions(): void {
    if (this.isRollbackAvailable_ && this.isRollbackRequested_) {
      this.powerwashMode_ = PowerwashMode.POWERWASH_WITH_ROLLBACK;
      this.classList.add('rollback-proposal-view');
    } else {
      this.powerwashMode_ = PowerwashMode.POWERWASH_ONLY;
      this.classList.remove('rollback-proposal-view');
    }
  }

  private onScreenStateChanged(): void {
    if (this.uiStep === ResetScreenUiState.REVERT_PROMISE) {
      getAnnouncerInstance().announce(this.i18n('resetRevertSpinnerMessage'));
      this.classList.add('revert-promise-view');
    } else {
      this.classList.remove('revert-promise-view');
    }
    this.inRevertState_ = this.uiStep === ResetScreenUiState.REVERT_PROMISE;
  }

  /**
   * Determines the subtitle based on the current powerwash mode
   */
  private getPowerwashStateSubtitle(_locale: string, _mode: PowerwashMode):
      string {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails!.subtitleText);
  }

  /**
   * The powerwash button text depends on the powerwash mode
   */
  private getPowerwashButtonTextKey(_locale: string, _mode: PowerwashMode):
      string {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    return POWERWASH_MODE_DETAILS.get(this.powerwashMode_)!.buttonTextKey;
  }

  /**
   * Cannot powerwash with rollback when the TPM update checkbox is checked
   */
  private isPowerwashDisabled(
      _mode: PowerwashMode, _tpmUpdateChecked: boolean): boolean {
    return this.tpmUpdateChecked_ &&
        (this.powerwashMode_ === PowerwashMode.POWERWASH_WITH_ROLLBACK);
  }

  /* ---------- CONFIRMATION DIALOG ---------- */

  /**
   * Determines the confirmation dialog title.
   */
  private getConfirmationDialogTitle(_locale: string, _mode: PowerwashMode):
      string {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails!.dialogTitle);
  }

  /**
   * Determines the confirmation dialog content
   */
  private getConfirmationDialogText(_locale: string, _mode: PowerwashMode):
      string {
    if (this.powerwashMode_ === undefined) {
      return '';
    }
    const modeDetails = POWERWASH_MODE_DETAILS.get(this.powerwashMode_);
    return this.i18n(modeDetails!.dialogContent);
  }

  /**
   * On-tap event handler for confirmation dialog continue button.
   */
  private onDialogContinueClick(): void {
    this.userActed('powerwash-pressed');
  }

  /**
   * On-tap event handler for confirmation dialog cancel button.
   */
  private onDialogCancelClick(): void {
    const confirmationDialog =
        this.shadowRoot!.querySelector<OobeModalDialog>('#confirmationDialog')!;
    confirmationDialog.hideDialog();
    this.userActed('reset-confirm-dismissed');
  }

  /**
   * Catch 'close' event through escape key
   */
  private onDialogClosed(): void {
    this.userActed('reset-confirm-dismissed');
  }

  /* ---------- SIMPLE EVENT HANDLERS ---------- */
  /**
   * On-tap event handler for cancel button.
   */
  private onCancelClick(): void {
    this.userActed('cancel-reset');
  }

  /**
   * On-tap event handler for restart button.
   */
  private onRestartClick(): void {
    this.userActed('restart-pressed');
  }

  /**
   * On-tap event handler for powerwash button.
   */
  private onPowerwashClick(): void {
    this.userActed('show-confirmation');
  }

  /**
   * On-tap event handler for learn more link.
   */
  private onLearnMoreClick(): void {
    this.userActed('learn-more-link');
  }

  /**
   * Change handler for TPM firmware update checkbox.
   */
  private onTpmFirmwareUpdateChanged(): void {
    const tpmFirmwareUpdateCheckbox =
        this.shadowRoot!.querySelector<CrCheckboxElement>(
            '#tpmFirmwareUpdateCheckbox')!;
    const checked = tpmFirmwareUpdateCheckbox.checked;
    this.userActed(['tpmfirmware-update-checked', checked]);
  }

  /**
   * On-tap event handler for the TPM firmware update learn more link.
   */
  private onTpmFirmwareUpdateLearnMore(event: Event): void {
    this.userActed('tpm-firmware-update-learn-more-link');
    event.stopPropagation();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeReset.is]: OobeReset;
  }
}

customElements.define(OobeReset.is, OobeReset);
