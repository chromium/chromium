// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/js/action_link.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/oobe_vars/oobe_shared_vars.css.js';
import '../../components/buttons/oobe_icon_button.js';
import '../../components/hd_iron_icon.js';
import '../../components/quick_start_entry_point.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeIconButton} from '../../components/buttons/oobe_icon_button.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {LongTouchDetector} from '../../components/long_touch_detector.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCrLottie} from '../../components/oobe_cr_lottie.js';

import {getTemplate} from './welcome_dialog.html.js';

const OobeWelcomeDialogBase =
    OobeDialogHostMixin(OobeI18nMixin(PolymerElement));

export class OobeWelcomeDialog extends OobeWelcomeDialogBase {
  static get is() {
    return 'oobe-welcome-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Currently selected system language (display name).
       */
      currentLanguage: {
        type: String,
      },

      /**
       * Controls visibility of "Timezone" button.
       */
      timezoneButtonVisible: {
        type: Boolean,
      },

      /**
       * Controls displaying of "Enable debugging features" link.
       */
      debuggingLinkVisible: {
        type: Boolean,
      },

      /**
       * Observer for when this screen is hidden, or shown.
       */
      hidden: {
        type: Boolean,
        observer: 'updateHidden',
        reflectToAttribute: true,
      },

      isMeet: {
        type: Boolean,
        value: function() {
          return (
              loadTimeData.valueExists('deviceFlowType') &&
              loadTimeData.getString('deviceFlowType') === 'meet');
        },
        readOnly: true,
      },

      isBootAnimation: {
        type: Boolean,
        value: function() {
          return (
              loadTimeData.valueExists('isBootAnimationEnabled') &&
              loadTimeData.getBoolean('isBootAnimationEnabled'));
        },
        readOnly: true,
      },

      isDeviceRequisitionConfigurable: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('isDeviceRequisitionConfigurable');
        },
        readOnly: true,
      },

      isOobeLoaded: {
        type: Boolean,
        value: false,
      },

      isQuickStartEnabled: {
        type: Boolean,
      },
    };
  }

  private currentLanguage: string;
  private timezoneButtonVisible: boolean;
  private debuggingLinkVisible: boolean;
  private isMeet: boolean;
  private isBootAnimation: boolean;
  private isDeviceRequisitionConfigurable: boolean;
  private isOobeLoaded: boolean;
  isQuickStartEnabled: boolean;

  private titleLongTouchDetector: LongTouchDetector | null;
  private focusedElement: string | null;

  constructor() {
    super();
    this.currentLanguage = '';
    this.timezoneButtonVisible = false;

    this.titleLongTouchDetector = null;

    /**
     * This is stored ID of currently focused element to restore id on returns
     * to this dialog from Language / Timezone Selection dialogs.
     */
    this.focusedElement = null;

    this.isQuickStartEnabled = false;
  }

  private getGetStartedButton(): OobeTextButton {
    const button = this.shadowRoot?.querySelector('#getStarted');
    assert(button instanceof OobeTextButton);
    return button;
  }

  private getEnableDebuggingButton(): OobeIconButton {
    const button = this.shadowRoot?.querySelector('#enableDebuggingButton');
    assert(button instanceof OobeIconButton);
    return button;
  }

  override ready() {
    super.ready();
    if (loadTimeData.getBoolean('isOobeLazyLoadingEnabled')) {
      // Disable the 'Get Started' & 'Enable Debugging' button until OOBE is
      // fully initialized.
      this.getGetStartedButton().disabled = true;
      this.getEnableDebuggingButton().disabled = true;
      document.addEventListener(
        'oobe-screens-loaded', this.enableButtonsWhenLoaded.bind(this));
    }
  }

  override onBeforeShow() {
    super.onBeforeShow();
    this.setVideoPlay(true);
  }

  /**
   * Since we prioritize the showing of the the Welcome Screen, it becomes
   * visible before the remaining of the OOBE flow is fully loaded. For this
   * reason, we listen to the |oobe-screens-loaded| signal and enable it.
   */
  private enableButtonsWhenLoaded(): void {
    document.removeEventListener(
      'oobe-screens-loaded', this.enableButtonsWhenLoaded.bind(this));
    this.getGetStartedButton().disabled = false;
    this.getEnableDebuggingButton().disabled = false;
    this.isOobeLoaded = true;
  }

  private onLanguageClicked(): void {
    this.focusedElement = 'languageSelectionButton';
    this.dispatchEvent(new CustomEvent('language-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onAccessibilityClicked(): void {
    this.focusedElement = 'accessibilitySettingsButton';
    this.dispatchEvent(new CustomEvent('accessibility-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onTimezoneClicked(): void {
    this.focusedElement = 'timezoneSettingsButton';
    this.dispatchEvent(new CustomEvent('timezone-button-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onNextClicked(): void {
    this.focusedElement = 'getStarted';
    this.dispatchEvent(new CustomEvent(
        'next-button-clicked', {bubbles: true, composed: true}));
  }

  private onDebuggingLinkClicked(): void {
    this.dispatchEvent(new CustomEvent('enable-debugging-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  /*
   * This is called from titleLongTouchDetector when long touch is detected.
   *
   */
  private onTitleLongTouch(): void {
    this.dispatchEvent(new CustomEvent('launch-advanced-options', {
      bubbles: true,
      composed: true,
    }));
  }

  override connectedCallback() {
    super.connectedCallback();
    // Allow opening advanced options only if it is a meet device or device
    // requisition is configurable.
    if (this.isMeet || this.isDeviceRequisitionConfigurable) {
      const title = this.shadowRoot?.querySelector('#title');
      assert(title instanceof HTMLElement);
      this.titleLongTouchDetector = new LongTouchDetector(
          title, () => void this.onTitleLongTouch());
    }
    this.getChromeVoxHintDialog().addEventListener('keydown', (event) => {
      // When the ChromeVox hint dialog is open, allow users to press the
      // space bar to activate ChromeVox. This is intended to help first time
      // users easily activate ChromeVox.
      if (this.getChromeVoxHintDialog().open && event.key === ' ') {
        this.activateChromeVox();
        event.preventDefault();
        event.stopPropagation();
      }
    });
    this.focus();
  }

  override focus() {
    if (!this.focusedElement) {
      this.focusedElement = 'getStarted';
    }
    const elem = this.shadowRoot?.querySelector('#' + this.focusedElement);
    assert(elem instanceof HTMLElement);
    elem.focus();
  }

  /*
   * Observer method for changes to the hidden property.
   * This replaces the show() function, in this class.
   */
  private updateHidden(newValue: boolean, _oldValue: boolean): void {
    const visible = !newValue;
    if (visible) {
      this.focus();
    }

    this.setVideoPlay(visible);
  }

  /**
   * Play or pause welcome video.
   * @param play - whether play or pause welcome video.
   */
  private setVideoPlay(play: boolean): void {
    // Postpone the call until OOBE is loaded, if necessary.
    if (!this.isOobeLoaded) {
      document.addEventListener(
        'oobe-screens-loaded', () => {
          this.isOobeLoaded = true;
          this.setVideoPlay(play);
        }, { once: true });
      return;
    }

    const welcomeAnimation =
        this.shadowRoot?.querySelector('#welcomeAnimation');
    if (welcomeAnimation instanceof OobeCrLottie) {
      welcomeAnimation.playing = play;
    }
  }

  /**
   * This function formats message for labels.
   * @param label i18n string ID.
   * @param parameter i18n string parameter.
   */
  private formatMessage(label: string, parameter: string): string {
    return loadTimeData.getStringF(label, parameter);
  }

  // ChromeVox hint section.

  private getChromeVoxHintDialog(): OobeModalDialog {
    const dialog = this.shadowRoot?.querySelector('#chromeVoxHint');
    assert(dialog instanceof OobeModalDialog);
    return dialog;
  }

  /**
   * Called to show the ChromeVox hint dialog.
   */
  showChromeVoxHint(): void {
    this.getChromeVoxHintDialog().showDialog();
    this.setVideoPlay(false);
  }

  /**
   * Called to close the ChromeVox hint dialog.
   */
  closeChromeVoxHint(): void {
    this.setVideoPlay(true);
    this.getChromeVoxHintDialog().hideDialog();
  }

  /**
   * Called when the 'Continue without ChromeVox' button is clicked.
   */
  private dismissChromeVoxHint(): void {
    this.dispatchEvent(new CustomEvent('chromevox-hint-dismissed', {
      bubbles: true,
      composed: true,
    }));
    this.closeChromeVoxHint();
  }

  private activateChromeVox(): void {
    this.closeChromeVoxHint();
    this.dispatchEvent(new CustomEvent('chromevox-hint-accepted', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * Determines if AnimationSlot is needed for specific flow
   */
  private showAnimationSlot(): boolean {
    return !this.isBootAnimation;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeWelcomeDialog.is]: OobeWelcomeDialog;
  }
}

customElements.define(OobeWelcomeDialog.is, OobeWelcomeDialog);
