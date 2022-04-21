// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CameraManager} from '../../device/index.js';
import {
  VideoResolutionOption,
  VideoResolutionOptionGroup,
} from '../../device/type.js';
import * as dom from '../../dom.js';
import {I18nString} from '../../i18n_string.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {Facing, ViewName} from '../../type.js';
import {instantiateTemplate, setupI18nElements} from '../../util.js';

import {BaseSettings} from './base.js';
import * as util from './util.js';

/**
 * Controller of video resolution settings.
 */
export class VideoResolutionSettings extends BaseSettings {
  private readonly menu: HTMLElement;

  constructor(readonly cameraManager: CameraManager) {
    super(ViewName.VIDEO_RESOLUTION_SETTINGS);

    this.menu = dom.getFrom(this.root, 'div.menu', HTMLDivElement);
    cameraManager.registerCameraUI({
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
  }

  private onOptionsUpdate(groups: VideoResolutionOptionGroup[]): void {
    util.clearMenu(this.menu);
    for (const {deviceId, facing, options} of groups) {
      util.addTextItemToMenu(
          this.menu, '#resolution-label-template',
          util.getLabelFromFacing(facing));

      if (options.length === 1) {
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
  }

  private addResolutionItem(
      deviceId: string, facing: Facing, option: VideoResolutionOption): void {
    const optionElement =
        instantiateTemplate('#video-resolution-item-template');
    const span = dom.getFrom(optionElement, 'span', HTMLSpanElement);
    const text = util.toVideoResoloutionOptionLabel(option.resolutionLevel);
    span.textContent = text;
    const deviceName =
        loadTimeData.getI18nMessage(util.getLabelFromFacing(facing));
    span.setAttribute('aria-label', `${deviceName} ${text}`);

    const resolution = option.resolutions[0];
    const input = dom.getFrom(optionElement, 'input', HTMLInputElement);
    input.dataset['width'] = resolution.width.toString();
    input.dataset['height'] = resolution.height.toString();
    input.dataset['facing'] = facing;
    input.name = `video-resolution-${deviceId}`;
    input.checked = option.checked;

    if (!input.checked) {
      input.addEventListener('click', (event) => {
        this.cameraManager.setPrefVideoResolutionLevel(
            deviceId, option.resolutionLevel);
        event.preventDefault();
      });
    }

    // TODO(b/215484798): Moves FPS toggle into video resolution settings.
    this.menu.appendChild(optionElement);
  }
}
