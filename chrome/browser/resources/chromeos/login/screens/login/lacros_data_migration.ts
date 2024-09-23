// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for lacros data migration screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_slide.js';

import {assert} from '//resources/js/assert.js';
import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {LacrosDataMigrationPageCallbackRouter, LacrosDataMigrationPageHandlerRemote} from '../../mojom-webui/screens_login.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './lacros_data_migration.html.js';

enum LacrosDataMigrationStep {
  PROGRESS = 'progress',
  ERROR = 'error',
}

const LacrosDataMigrationScreenElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

export class LacrosDataMigrationScreen
    extends LacrosDataMigrationScreenElementBase {
  static get is() {
    return 'lacros-data-migration-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      progressValue: {
        type: Number,
        value: 0,
      },

      canSkip: {
        type: Boolean,
        value: false,
      },

      lowBatteryStatus: {
        type: Boolean,
        value: false,
      },

      requiredSizeStr: {
        type: String,
        value: '',
      },

      showGotoFiles: {
        type: Boolean,
        value: false,
      },
    };
  }

  private progressValue: number;
  private canSkip: boolean;
  private lowBatteryStatus: boolean;
  private requiredSizeStr: string;
  private showGotoFiles: boolean;
  private callbackRouter: LacrosDataMigrationPageCallbackRouter;
  private handler: LacrosDataMigrationPageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new LacrosDataMigrationPageCallbackRouter();
    this.handler = new LacrosDataMigrationPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishLacrosDataMigrationScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });
    this.callbackRouter.setProgressValue.addListener(
        this.setProgressValue.bind(this));
    this.callbackRouter.showSkipButton.addListener(
        this.showSkipButton.bind(this));
    this.callbackRouter.setLowBatteryStatus.addListener(
        this.setLowBatteryStatus.bind(this));
    this.callbackRouter.setFailureStatus.addListener(
        this.setFailureStatus.bind(this));
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return LacrosDataMigrationStep.PROGRESS;
  }

  override get UI_STEPS() {
    return LacrosDataMigrationStep;
  }

  /**
   * Called when the migration failed.
   * @param requiredSizeStr The extra space that users need to free up
   *     to run the migration formatted into a string. Maybe empty, if the
   *     failure is not caused by low disk space.
   * @param showGotoFiles If true, displays the "goto files" button.
   */
  setFailureStatus(requiredSizeStr: String16, showGotoFiles: boolean): void {
    this.requiredSizeStr = String.fromCharCode(...requiredSizeStr.data);
    this.showGotoFiles = showGotoFiles;
    this.setUIStep(LacrosDataMigrationStep.ERROR);
  }

  /**
   * Called to update the progress of data migration.
   * @param progress Percentage of data copied so far.
   */
  setProgressValue(progress: number): void {
    this.progressValue = progress;
  }

  /**
   * Called to make the skip button visible.
   */
  showSkipButton(): void {
    this.canSkip = true;
  }

  /**
   * Called on updating low battery status.
   * @param status Whether or not low-battery UI should
   *   show. Specifically, if battery is low and no charger is connected.
   */
  setLowBatteryStatus(status: boolean): void {
    this.lowBatteryStatus = status;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('LacrosDataMigrationScreen');
  }

  private onSkipButtonClicked(): void {
    assert(this.canSkip);
    this.handler.onSkipButtonClicked();
  }

  private onCancelButtonClicked(): void {
    this.handler.onCancelButtonClicked();
  }

  private onGotoFilesButtonClicked(): void {
    this.handler.onGotoFilesButtonClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [LacrosDataMigrationScreen.is]: LacrosDataMigrationScreen;
  }
}

customElements.define(LacrosDataMigrationScreen.is, LacrosDataMigrationScreen);
