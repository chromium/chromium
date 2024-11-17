// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CameraManager} from '../../device/index.js';
import {
  PhotoResolutionOption,
  PhotoResolutionOptionGroup,
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
 * View controller of photo resolution settings.
 */
export class PhotoResolutionSettings extends BaseSettings {
  private readonly menu: HTMLElement;

  private focusedDeviceId: string|null = null;

  private menuScrollTop = 0;

  constructor(readonly cameraManager: CameraManager) {
    super(ViewName.PHOTO_RESOLUTION_SETTINGS);

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

    this.cameraManager.addPhotoResolutionOptionListener(
        (groups) => this.onOptionsUpdate(groups));
  }

  private onOptionsUpdate(groups: PhotoResolutionOptionGroup[]): void {
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
    this.menu.scrollTop = this.menuScrollTop;
  }

  private addResolutionItem(
      deviceId: string, facing: Facing, option: PhotoResolutionOption): void {
    const optionElement = instantiateTemplate('#resolution-item-template');

    const label = util.toPhotoResolutionOptionLabel(option.resolutionLevel);
    const resolution = option.resolutions[0];
    let megaPixels = resolution.mp;
    if (this.cameraManager.useSquareResolution()) {
      const croppedEdge = Math.min(resolution.width, resolution.height);
      megaPixels = (new Resolution(croppedEdge, croppedEdge)).mp;
    }
    const mpInfo =
        loadTimeData.getI18nMessage(I18nString.LABEL_RESOLUTION_MP, megaPixels);
    const text = `${label} (${mpInfo})`;
    const span = dom.getFrom(optionElement, 'span', HTMLSpanElement);
    span.textContent = text;
    const deviceName =
        loadTimeData.getI18nMessage(util.getLabelFromFacing(facing));
    span.setAttribute('aria-label', `${deviceName} ${text}`);

    const input = dom.getFrom(optionElement, 'input', HTMLInputElement);
    input.dataset['width'] = resolution.width.toString();
    input.dataset['height'] = resolution.height.toString();
    input.dataset['facing'] = facing;
    input.name = `photo-resolution-${deviceId}`;
    input.checked = option.checked;

    if (!input.checked) {
      input.addEventListener('click', async (event) => {
        event.preventDefault();
        this.focusedDeviceId = deviceId;
        this.menuScrollTop = this.menu.scrollTop;
        if (expert.isEnabled(expert.ExpertOption.SHOW_ALL_RESOLUTIONS)) {
          await this.cameraManager.setPrefPhotoResolution(deviceId, resolution);
        } else {
          await this.cameraManager.setPrefPhotoResolutionLevel(
              deviceId, option.resolutionLevel);
        }
      });
    }
    this.menu.appendChild(optionElement);

    if (input.checked && this.focusedDeviceId === deviceId) {
      input.focus();
    }
  }
}
