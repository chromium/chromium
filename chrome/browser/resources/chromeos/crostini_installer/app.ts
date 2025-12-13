// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-progress/paper-progress.js';
import '/strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {CrSliderElement} from 'chrome://resources/ash/common/cr_elements/cr_slider/cr_slider.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';
import {DiskSliderTick, InstallerError, InstallerState} from './crostini_types.mojom-webui.js';

/**
 * Enum for the state of `crostini-installer-app`. Not to confused with
 * `installerState`.
 */
enum State {
  PROMPT = 'prompt',
  CONFIGURE = 'configure',
  INSTALLING = 'installing',
  ERROR = 'error',
  CANCELING = 'canceling',
}

const MAX_USERNAME_LENGTH: number = 32;
const NoDiskSpaceError: string = 'no_disk_space';

const UNAVAILABLE_USERNAMES: string[] = [
  'root',
  'daemon',
  'bin',
  'sys',
  'sync',
  'games',
  'man',
  'lp',
  'mail',
  'news',
  'uucp',
  'proxy',
  'www-data',
  'backup',
  'list',
  'irc',
  'gnats',
  'nobody',
  '_apt',
  'systemd-timesync',
  'systemd-network',
  'systemd-resolve',
  'systemd-bus-proxy',
  'messagebus',
  'sshd',
  'rtkit',
  'pulse',
  'android-root',
  'chronos-access',
  'android-everybody',
];

interface CrostiniInstallerAppElement {
  $: {
    username: CrInputElement,
  };
}

class CrostiniInstallerAppElement extends PolymerElement {
  static get is() {
    return 'crostini-installer-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      state_: {
        type: String,
        value: State.PROMPT,
      },

      error_: {
        type: String,
        value: InstallerError.kNone,
      },

      installerState_: {
        type: Number,
      },

      installerProgress_: {
        type: Number,
      },

      errorMessage_: {
        type: String,
      },

      /* Enable the html template to use State. */
      stateEnum_: {
        type: Object,
        value: State,
      },

      minDisk_: {
        type: String,
      },

      maxDisk_: {
        type: String,
      },

      defaultDiskSizeTick_: {
        type: Number,
      },

      diskSizeTicks_: {
        type: Array,
      },

      chosenDiskSize_: {
        type: Number,
      },

      isLowSpaceAvailable_: {
        type: Boolean,
      },

      showDiskSlider_: {
        type: Boolean,
        value: false,
      },

      username_: {
        type: String,
        value: loadTimeData.getString('defaultContainerUsername')
                   .substring(0, MAX_USERNAME_LENGTH),
        observer: 'onUsernameChanged_',
      },

      usernameError_: {
        type: String,
      },

      /* Enable the html template to access the length */
      MAX_USERNAME_LENGTH: {
        type: Number,
        value: MAX_USERNAME_LENGTH,
      },
    };
  }

  private state_: State;
  private error_: InstallerError|string;
  private installerState_: InstallerState;
  private installerProgress_: number;
  private errorMessage_: string;
  private minDisk_: string;
  private maxDisk_: string;
  private defaultDiskSizeTick_: number;
  private diskSizeTicks_: DiskSliderTick[];
  private chosenDiskSize_: number;
  private isLowSpaceAvailable_: boolean;
  private showDiskSlider_: boolean;
  private username_: string;
  private usernameError_: string;
  private MAX_USERNAME_LENGTH: number;
  private listenerIds_: number[];
  private diskSpacePromise_: Promise<{
    ticks: DiskSliderTick[],
    defaultIndex: number,
    isLowSpaceAvailable: boolean,
  }>;
  private onNextButtonClickIsRunning_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = BrowserProxy.getInstance().callbackRouter;

    this.listenerIds_ = [
      callbackRouter.onProgressUpdate.addListener(
          (installerState: InstallerState, progressFraction: number) => {
            this.installerState_ = installerState;
            this.installerProgress_ = progressFraction * 100;
          }),
      callbackRouter.onInstallFinished.addListener((error: InstallerError) => {
        if (error === InstallerError.kNone) {
          // Install succeeded.
          this.closePage_();
        } else {
          assert(this.state_ === State.INSTALLING);
          this.errorMessage_ = this.getErrorMessage_(error);
          this.error_ = error;
          this.state_ = State.ERROR;
        }
      }),
      callbackRouter.onCanceled.addListener(() => this.closePage_()),
      callbackRouter.requestClose.addListener(() => this.cancelOrBack_(true)),
    ];

    // Query the disk space sooner than later to minimize delay.
    this.diskSpacePromise_ =
        BrowserProxy.getInstance().handler.requestAmountOfFreeDiskSpace();

    document.addEventListener('keyup', event => {
      if (event.key === 'Escape') {
        this.cancelOrBack_();
        event.preventDefault();
      }
    });

    this.shadowRoot!.querySelector<HTMLElement>(
                        '.action-button:not([hidden])')!.focus();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    const callbackRouter = BrowserProxy.getInstance().callbackRouter;
    this.listenerIds_.forEach(id => callbackRouter.removeListener(id));
  }

  private async onNextButtonClick_() {
    if (!this.onNextButtonClickIsRunning_) {
      assert(this.state_ === State.PROMPT);
      this.onNextButtonClickIsRunning_ = true;

      // We should get the disk space very soon (if we have not already got it)
      // so the user will at worst see a very short delay.
      const diskSpace = await this.diskSpacePromise_;
      const ticks = diskSpace.ticks;

      if (ticks.length === 0) {
        this.errorMessage_ =
            loadTimeData.getString('minimumFreeSpaceUnmetError');
        this.error_ = NoDiskSpaceError;
        this.state_ = State.ERROR;

        this.onNextButtonClickIsRunning_ = false;
        return;
      }


      this.defaultDiskSizeTick_ = diskSpace.defaultIndex;
      this.diskSizeTicks_ = ticks;

      this.minDisk_ = ticks[0].label;
      this.maxDisk_ = ticks[ticks.length - 1].label;

      this.isLowSpaceAvailable_ = diskSpace.isLowSpaceAvailable;
      if (this.isLowSpaceAvailable_) {
        this.showDiskSlider_ = true;
      }

      this.state_ = State.CONFIGURE;
      // Focus the username input and move the cursor to the end.
      this.$.username.select(this.username_.length, this.username_.length);

      this.onNextButtonClickIsRunning_ = false;
    }
  }

  private onInstallButtonClick_() {
    assert(this.showInstallButton_());
    let diskSize: bigint = 0n;
    if (this.showDiskSlider_) {
      const slider =
          this.shadowRoot!.querySelector<CrSliderElement>('#diskSlider')!;
      diskSize = this.diskSizeTicks_[slider.value].value;
    } else {
      diskSize = this.diskSizeTicks_[this.defaultDiskSizeTick_].value;
    }
    this.installerState_ = InstallerState.kStart;
    this.installerProgress_ = 0;
    this.state_ = State.INSTALLING;
    BrowserProxy.getInstance().handler.install(
        BigInt(diskSize), this.username_);
  }

  private onSettingsButtonClick_() {
    window.open('chrome://os-settings/help');
  }

  private onCancelButtonClick_() {
    this.cancelOrBack_();
  }

  private cancelOrBack_(forceCancel: boolean = false) {
    switch (this.state_) {
      case State.PROMPT:
        BrowserProxy.getInstance().handler.cancelBeforeStart();
        this.closePage_();
        break;
      case State.CONFIGURE:
        if (forceCancel) {
          this.closePage_();
        } else {
          this.state_ = State.PROMPT;
        }
        break;
      case State.INSTALLING:
        this.state_ = State.CANCELING;
        BrowserProxy.getInstance().handler.cancel();
        break;
      case State.ERROR:
        this.closePage_();
        break;
      case State.CANCELING:
        // Although cancel button has been disabled, we can reach here if users
        // press <esc> key or from mojom "RequestClose()".
        break;
      default:
        assertNotReached();
    }
  }

  private closePage_() {
    BrowserProxy.getInstance().handler.onPageClosed();
  }

  private getTitle_(state: State, error: InstallerError): string {
    let titleId: string = '';
    switch (state) {
      case State.PROMPT:
      case State.CONFIGURE:
        titleId = 'promptTitle';
        break;
      case State.INSTALLING:
        titleId = 'installingTitle';
        break;
      case State.ERROR:
        // eslint-disable-next-line eqeqeq
        if (error == InstallerError.kNeedUpdate) {
          titleId = 'needUpdateTitle';
        } else {
          titleId = 'errorTitle';
        }
        break;
      case State.CANCELING:
        titleId = 'cancelingTitle';
        break;
      default:
        assertNotReached();
    }
    return loadTimeData.getString(titleId);
  }

  private eq_(value1: any, value2: any): boolean {
    return value1 === value2;
  }

  private showInstallButton_(): boolean {
    return this.state_ === State.CONFIGURE ||
        (this.state_ === State.ERROR && this.error_ !== NoDiskSpaceError &&
         // eslint-disable-next-line eqeqeq
         this.error_ != InstallerError.kNeedUpdate);
  }

  private disableInstallButton_(): boolean {
    if (this.state_ === State.CONFIGURE) {
      return !this.username_ || !!this.usernameError_;
    }
    return false;
  }

  private showNextButton_(): boolean {
    return this.state_ === State.PROMPT;
  }

  private showSettingsButton_(): boolean {
    // eslint-disable-next-line eqeqeq
    return this.state_ === State.ERROR &&
        this.error_ == InstallerError.kNeedUpdate;
  }

  private getInstallButtonLabel_(): string {
    switch (this.state_) {
      case State.CONFIGURE:
        return loadTimeData.getString('install');
      case State.ERROR:
        return loadTimeData.getString('retry');
      default:
        return '';
    }
  }

  private getProgressMessage_(): string {
    let messageId: string = '';
    switch (this.installerState_) {
      case InstallerState.kStart:
        break;
      case InstallerState.kInstallImageLoader:
        messageId = 'loadTerminaMessage';
        break;
      case InstallerState.kCreateDiskImage:
        messageId = 'createDiskImageMessage';
        break;
      case InstallerState.kStartTerminaVm:
        messageId = 'startTerminaVmMessage';
        break;
      case InstallerState.kStartLxd:
        messageId = 'startLxdMessage';
        break;
      case InstallerState.kCreateContainer:
      case InstallerState.kSetupContainer:
        messageId = 'setupContainerMessage';
        break;
      case InstallerState.kStartContainer:
        messageId = 'startContainerMessage';
        break;
      case InstallerState.kConfigureContainer:
        messageId = 'configureContainerMessage';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  }

  private getErrorMessage_(error: InstallerError): string {
    let messageId: string = '';
    switch (error) {
      case InstallerError.kErrorLoadingTermina:
        messageId = 'loadTerminaError';
        break;
      case InstallerError.kNeedUpdate:
        messageId = 'needUpdateError';
        break;
      case InstallerError.kErrorCreatingDiskImage:
        messageId = 'createDiskImageError';
        break;
      case InstallerError.kErrorStartingTermina:
        messageId = 'startTerminaVmError';
        break;
      case InstallerError.kErrorStartingLxd:
        messageId = 'startLxdError';
        break;
      case InstallerError.kErrorStartingContainer:
        messageId = 'startContainerError';
        break;
      case InstallerError.kErrorConfiguringContainer:
        messageId = 'configureContainerError';
        break;
      case InstallerError.kErrorOffline:
        messageId = 'offlineError';
        break;
      case InstallerError.kErrorSettingUpContainer:
        messageId = 'setupContainerError';
        break;
      case InstallerError.kErrorInsufficientDiskSpace:
        messageId = 'insufficientDiskError';
        break;
      case InstallerError.kErrorCreateContainer:
        messageId = 'setupContainerError';
        break;
      case InstallerError.kErrorUnknown:
        messageId = 'unknownError';
        break;
      default:
        assertNotReached();
    }

    return messageId ? loadTimeData.getString(messageId) : '';
  }

  private onUsernameChanged_() {
    if (!this.username_) {
      this.usernameError_ = '';
    } else if (UNAVAILABLE_USERNAMES.includes(this.username_)) {
      this.usernameError_ =
          loadTimeData.getStringF('usernameNotAvailableError', this.username_);
    } else if (!/^[a-z_]/.test(this.username_)) {
      this.usernameError_ =
          loadTimeData.getString('usernameInvalidFirstCharacterError');
    } else if (!/^[a-z0-9_-]*$/.test(this.username_)) {
      this.usernameError_ =
          loadTimeData.getString('usernameInvalidCharactersError');
    } else {
      this.usernameError_ = '';
    }
  }

  private getCancelButtonLabel_(): string {
    return loadTimeData.getString(
        this.state_ === State.CONFIGURE ? 'back' : 'cancel');
  }

  private showErrorMessage_(): boolean {
    return this.state_ === State.ERROR;
  }

  private onDiskSizeRadioChanged_(event: CustomEvent<{value: string}>) {
    this.showDiskSlider_ =
        (event.detail.value !== 'recommended' || !!this.isLowSpaceAvailable_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'crostini-installer-app': CrostiniInstallerAppElement;
  }
}

customElements.define(
    CrostiniInstallerAppElement.is, CrostiniInstallerAppElement);
