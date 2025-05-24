// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../../assert.js';
import {CameraManager} from '../../device/index.js';
import {
  BaseSettingsOption,
  BaseSettingsOptionGroup,
} from '../../device/type.js';
import * as dom from '../../dom.js';
import {setExpertMode} from '../../expert.js';
import {I18nString} from '../../i18n_string.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {ChromeHelper} from '../../mojo/chrome_helper.js';
import * as nav from '../../nav.js';
import * as scannerChip from '../../scanner_chip.js';
import * as state from '../../state.js';
import {Mode, ViewName} from '../../type.js';
import * as util from '../../util.js';
import {View} from '../view.js';

import {BaseSettings} from './base.js';
import {PhotoAspectRatioSettings} from './photo_aspect_ratio.js';
import {PhotoResolutionSettings} from './photo_resolution.js';
import {
  toAspectRatioAriaLabel,
  toAspectRatioLabel,
  toPhotoResolutionOptionLabel,
  toVideoResolutionOptionLabel,
} from './util.js';
import {VideoResolutionSettings} from './video_resolution.js';

const helpUrl =
    'https://support.google.com/chromebook/?p=camera_usage_on_chromebook';

function bindButton(openerId: string, callback: () => void): void {
  const opener = dom.get(`#${openerId}`, HTMLElement);
  opener.addEventListener('click', () => {
    callback();
  });
}

/**
 * Controller of primary settings view.
 */
export class PrimarySettings extends BaseSettings {
  private readonly subViews: BaseSettings[];

  private readonly header: HTMLElement;

  private readonly photoResolutionSettings: HTMLButtonElement;

  private readonly photoAspectRatioSettings: HTMLButtonElement;

  private readonly videoResolutionSettings: HTMLButtonElement;

  private headerClickedCount = 0;

  private headerClickedLastTime: number|null = null;

  constructor(readonly cameraManager: CameraManager) {
    super(ViewName.SETTINGS);

    bindButton(
        'settings-photo-resolution',
        () => this.openSubSettings(ViewName.PHOTO_RESOLUTION_SETTINGS));
    bindButton(
        'settings-photo-aspect-ratio',
        () => this.openSubSettings(ViewName.PHOTO_ASPECT_RATIO_SETTINGS));
    bindButton(
        'settings-video-resolution',
        () => this.openSubSettings(ViewName.VIDEO_RESOLUTION_SETTINGS));
    bindButton(
        'settings-expert',
        () => this.openSubSettings(ViewName.EXPERT_SETTINGS));
    bindButton('settings-feedback', () => {
      // Prevent setting view overlapping preview when sending app
      // window feedback screenshot b/155938542.
      this.leave();
      ChromeHelper.getInstance().openFeedbackDialog(loadTimeData.getI18nMessage(
          I18nString.FEEDBACK_DESCRIPTION_PLACEHOLDER));
    });
    bindButton('settings-help', () => {
      ChromeHelper.getInstance().openUrlInBrowser(helpUrl);
    });

    this.photoResolutionSettings =
        dom.get(`#settings-photo-resolution`, HTMLButtonElement);
    this.photoAspectRatioSettings =
        dom.get(`#settings-photo-aspect-ratio`, HTMLButtonElement);
    this.videoResolutionSettings =
        dom.get(`#settings-video-resolution`, HTMLButtonElement);

    this.subViews = [
      new PhotoResolutionSettings(this.cameraManager),
      new PhotoAspectRatioSettings(this.cameraManager),
      new VideoResolutionSettings(this.cameraManager),
      new BaseSettings(ViewName.EXPERT_SETTINGS),
    ];

    this.header = dom.get('#settings-header', HTMLElement);
    this.header.addEventListener('click', () => this.onHeaderClicked());

    const cameraSettings = [
      this.photoResolutionSettings,
      this.photoAspectRatioSettings,
      this.videoResolutionSettings,
    ];

    cameraManager.registerCameraUi({
      onCameraUnavailable: () => {
        for (const setting of cameraSettings) {
          setting.disabled = true;
        }
      },
      onCameraAvailable: () => {
        for (const setting of cameraSettings) {
          setting.disabled = false;
        }
      },
    });

    this.cameraManager.addPhotoResolutionOptionListener((groups) => {
      const option = this.getSelectedOption(groups);
      if (option === null) {
        return;
      }
      const span =
          dom.getFrom(this.photoResolutionSettings, 'span', HTMLSpanElement);
      span.textContent = toPhotoResolutionOptionLabel(option.resolutionLevel);
    });
    this.cameraManager.addPhotoAspectRatioOptionListener((groups) => {
      const option = this.getSelectedOption(groups);
      if (option === null) {
        return;
      }
      const span =
          dom.getFrom(this.photoAspectRatioSettings, 'span', HTMLSpanElement);
      span.textContent = toAspectRatioLabel(option.aspectRatioSet);
      span.setAttribute(
          'aria-label', toAspectRatioAriaLabel(option.aspectRatioSet));
    });
    this.cameraManager.addVideoResolutionOptionListener((groups) => {
      const option = this.getSelectedOption(groups);
      if (option === null) {
        return;
      }
      const span =
          dom.getFrom(this.videoResolutionSettings, 'span', HTMLSpanElement);
      span.textContent = toVideoResolutionOptionLabel(option.resolutionLevel);
    });

    state.addObserver(state.State.ENABLE_PREVIEW_OCR, (enabled) => {
      if (!enabled) {
        scannerChip.dismiss();
      }
    });
  }

  private getSelectedOption<T extends BaseSettingsOption>(
      groups: Array<BaseSettingsOptionGroup<T>>): T|null {
    const currentGroup = groups.find(
        (group) => group.deviceId === this.cameraManager.getDeviceId());
    // No camera is ready so no information to show.
    if (currentGroup === undefined) {
      return null;
    }
    const selectedOption =
        currentGroup.options.find((option) => option.checked);
    assert(selectedOption !== undefined);
    return selectedOption;
  }

  /**
   * Handle click on primary settings header (used to trigger expert mode).
   */
  private onHeaderClicked() {
    const reset = () => {
      this.headerClickedCount = 0;
      this.headerClickedLastTime = null;
    };

    // Reset the counter if last click is more than 1 second ago.
    if (this.headerClickedLastTime !== null &&
        (Date.now() - this.headerClickedLastTime) > 1000) {
      reset();
    }

    this.headerClickedCount++;
    this.headerClickedLastTime = Date.now();

    if (this.headerClickedCount === 5) {
      setExpertMode(true);
      reset();
    }
  }

  override getSubViews(): View[] {
    return this.subViews;
  }

  override entering(): void {
    this.updateHeader();
  }

  private updateHeader(): void {
    const headerString = state.get(Mode.VIDEO) ? I18nString.VIDEO_SETTINGS :
                                                 I18nString.PHOTO_SETTINGS;
    this.header.setAttribute('i18n-text', headerString);
    util.setupI18nElements(assertInstanceof(this.header, HTMLElement));
  }

  private async openSubSettings(name: ViewName): Promise<void> {
    // Dismiss primary-settings if sub-settings was dismissed by background
    // click.
    const cond = await nav.open(name).closed;
    if (cond.kind === 'BACKGROUND_CLICKED') {
      this.leave(cond);
    }
  }
}
