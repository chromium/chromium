// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from '../../assert.js';
import {CameraManager} from '../../device/index.js';
import {
  SUPPORTED_CONSTANT_FPS,
  VideoResolutionOption,
  VideoResolutionOptionGroup,
} from '../../device/type.js';
import * as dom from '../../dom.js';
import * as expert from '../../expert.js';
import {I18nString} from '../../i18n_string.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {Facing, Resolution, ViewName} from '../../type.js';
import {instantiateTemplate, setupI18nElements} from '../../util.js';

import {BaseSettings} from './base.js';
import * as util from './util.js';

/**
 * Controller of video resolution settings.
 */
export class VideoResolutionSettings extends BaseSettings {
  private readonly menu: HTMLElement;

  private focusedDeviceId: string|null = null;

  private menuScrollTop = 0;

  constructor(readonly cameraManager: CameraManager) {
    super(ViewName.VIDEO_RESOLUTION_SETTINGS);

    this.menu = dom.getFrom(this.root, 'div.menu', HTMLDivElement);
    cameraManager.registerCameraUi({
      onCameraUnavailable: () => {
        for (const input of dom.getAllFrom(
                 this.menu, 'input', HTMLInputElement)) {
          input.disabled = true;
        }
      },
      onCameraAvailable: () => {
        for (const input of dom.getAllFrom(
                 this.menu, 'input', HTMLInputElement)) {
          input.disabled = false;
        }
      },
    });

    this.cameraManager.addVideoResolutionOptionListener(
        (groups) => this.onOptionsUpdate(groups));

    expert.addObserver(
        expert.ExpertOption.ENABLE_FPS_PICKER_FOR_BUILTIN,
        () => this.toggleFpsPickerVisiblity);
  }

  private onOptionsUpdate(groups: VideoResolutionOptionGroup[]): void {
    util.clearMenu(this.menu);
    for (const {deviceId, facing, options} of groups) {
      util.addTextItemToMenu(
          this.menu, '#resolution-label-template',
          util.getLabelFromFacing(facing));

      if (options.length === 1 &&
          this.getSupportedConstFpsOptionsLength(options[0]) <= 1) {
        util.addTextItemToMenu(
            this.menu, '#resolution-text-template',
            I18nString.LABEL_NO_RESOLUTION_OPTION);
      } else {
        for (const option of options) {
          this.addResolutionItem(deviceId, facing, option);
        }
      }
    }
    setupI18nElements(this.menu);
    this.menu.scrollTop = this.menuScrollTop;
  }

  private getSupportedConstFpsOptionsLength(option: VideoResolutionOption):
      number {
    return option.fpsOptions
        .filter(
            (fpsOption) => fpsOption.constFps !== null &&
                SUPPORTED_CONSTANT_FPS.includes(fpsOption.constFps))
        .length;
  }

  private addResolutionItem(
      deviceId: string, facing: Facing, option: VideoResolutionOption): void {
    const optionElement =
        instantiateTemplate('#video-resolution-item-template');
    const span = dom.getFrom(optionElement, 'span', HTMLSpanElement);

    let text;
    const label = util.toVideoResolutionOptionLabel(option.resolutionLevel);
    if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
      const mpInfo = loadTimeData.getI18nMessage(
          I18nString.LABEL_RESOLUTION_MP,
          option.fpsOptions[0].resolutions[0].mp);
      text = `${label} (${mpInfo})`;
    } else {
      text = label;
    }
    span.textContent = text;
    const deviceName =
        loadTimeData.getI18nMessage(util.getLabelFromFacing(facing));
    span.setAttribute('aria-label', `${deviceName} ${text}`);

    // Currently FPS buttons are only supported on external cameras.
    const constFpsOptionsLength =
        this.getSupportedConstFpsOptionsLength(option);
    let resolution: Resolution|null = null;
    for (const fps of SUPPORTED_CONSTANT_FPS) {
      const fpsButton =
          dom.getFrom(optionElement, `.fps-${fps}`, HTMLButtonElement);
      if (constFpsOptionsLength <= 1) {
        fpsButton.classList.add('invisible');
        fpsButton.hidden = true;
      } else if (facing === Facing.EXTERNAL) {
        fpsButton.hidden = false;
      } else {
        fpsButton.hidden = !expert.isEnabled(
            expert.ExpertOption.ENABLE_FPS_PICKER_FOR_BUILTIN);
      }
      const fpsOption =
          option.fpsOptions.find((fpsOption) => fpsOption.constFps === fps);
      const checked = fpsOption?.checked ?? false;
      fpsButton.classList.toggle('checked', checked);
      if (!checked) {
        fpsButton.addEventListener('click', async () => {
          // We don't want to reconfigure the stream when changing the FPS
          // preference for resolution level which is not currently selected.
          const shouldReconfigure =
              option.checked && this.cameraManager.getDeviceId() === deviceId;
          await this.cameraManager.setPrefVideoConstFps(
              deviceId, option.resolutionLevel, fps, shouldReconfigure);
        });
      } else {
        resolution = fpsOption?.resolutions[0] ?? null;
      }
    }
    // For cases that constant frame rate is not supported on the device
    // (e.g. Betty or legacy devices migrated from camera HAL v1), use the
    // resolution from the non-constant fps option.
    if (resolution === null) {
      const nonConstantFpsOption =
          option.fpsOptions.find((fpsOption) => fpsOption.constFps === null);
      resolution = nonConstantFpsOption?.resolutions[0] ?? null;
      assert(resolution !== null);
    }

    const input = dom.getFrom(optionElement, 'input', HTMLInputElement);
    input.dataset['width'] = resolution.width.toString();
    input.dataset['height'] = resolution.height.toString();
    input.dataset['facing'] = facing;
    input.name = `video-resolution-${deviceId}`;
    input.checked = option.checked;

    if (!input.checked) {
      input.addEventListener('click', async (event) => {
        event.preventDefault();
        this.focusedDeviceId = deviceId;
        this.menuScrollTop = this.menu.scrollTop;
        if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
          await this.cameraManager.setPrefVideoResolution(
              deviceId, assertExists(resolution));
        } else {
          await this.cameraManager.setPrefVideoResolutionLevel(
              deviceId, option.resolutionLevel);
        }
      });
    }
    this.menu.appendChild(optionElement);

    if (input.checked && this.focusedDeviceId === deviceId) {
      input.focus();
    }
  }

  private toggleFpsPickerVisiblity(): void {
    const isFpsEnabled =
        expert.isEnabled(expert.ExpertOption.ENABLE_FPS_PICKER_FOR_BUILTIN);
    const fpsButtons =
        dom.getAllFrom(this.menu, '.fps-buttons button', HTMLButtonElement);
    for (const fpsButton of fpsButtons) {
      fpsButton.hidden = !isFpsEnabled;
    }
  }
}
