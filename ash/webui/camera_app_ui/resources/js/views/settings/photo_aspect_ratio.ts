// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CameraManager} from '../../device/index.js';
import {
  PhotoAspectRatioOption,
  PhotoAspectRatioOptionGroup,
} from '../../device/type.js';
import * as dom from '../../dom.js';
import * as loadTimeData from '../../models/load_time_data.js';
import {Facing, ViewName} from '../../type.js';
import {instantiateTemplate, setupI18nElements} from '../../util.js';

import {BaseSettings} from './base.js';
import * as util from './util.js';

/**
 * View controller of photo aspect ratio settings.
 */
export class PhotoAspectRatioSettings extends BaseSettings {
  private readonly menu: HTMLElement;

  private focusedDeviceId: string|null = null;

  private menuScrollTop = 0;

  constructor(readonly cameraManager: CameraManager) {
    super(ViewName.PHOTO_ASPECT_RATIO_SETTINGS);

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

    this.cameraManager.addPhotoAspectRatioOptionListener(
        (groups) => this.onOptionsUpdate(groups));
  }

  private onOptionsUpdate(groups: PhotoAspectRatioOptionGroup[]): void {
    util.clearMenu(this.menu);
    for (const {deviceId, facing, options} of groups) {
      const deviceLabel = util.getLabelFromFacing(facing);
      const labelOption = instantiateTemplate('#resolution-label-template');
      dom.getFrom(labelOption, 'div', HTMLDivElement)
          .setAttribute('i18n-aria', deviceLabel);
      dom.getFrom(labelOption, 'span', HTMLSpanElement).textContent =
          loadTimeData.getI18nMessage(deviceLabel);
      this.menu.appendChild(labelOption);

      // After moving square mode option into aspect ratio settings, generally
      // it is impossible to have less than two options since "square" will
      // always be an option.
      for (const option of options) {
        this.addAspectRatioItem(deviceId, facing, option);
      }
    }
    setupI18nElements(this.menu);
    this.menu.scrollTop = this.menuScrollTop;
  }

  private addAspectRatioItem(
      deviceId: string, facing: Facing, option: PhotoAspectRatioOption): void {
    const deviceLabel = util.getLabelFromFacing(facing);
    const optionElement = instantiateTemplate('#resolution-item-template');
    const span = dom.getFrom(optionElement, 'span', HTMLSpanElement);
    span.textContent = util.toAspectRatioLabel(option.aspectRatioSet);
    const deviceName = loadTimeData.getI18nMessage(deviceLabel);
    span.setAttribute(
        'aria-label',
        `${deviceName} ${util.toAspectRatioAriaLabel(option.aspectRatioSet)}`);

    const input = dom.getFrom(optionElement, 'input', HTMLInputElement);
    input.dataset['aspectRatio'] = option.aspectRatioSet.toString();
    input.dataset['facing'] = facing;
    input.name = `photo-aspect-ratio-${deviceId}`;
    input.checked = option.checked;

    if (!input.checked) {
      input.addEventListener('click', async (event) => {
        event.preventDefault();
        this.focusedDeviceId = deviceId;
        this.menuScrollTop = this.menu.scrollTop;
        await this.cameraManager.setPrefPhotoAspectRatioSet(
            deviceId, option.aspectRatioSet);
      });
    }
    this.menu.appendChild(optionElement);

    if (input.checked && this.focusedDeviceId === deviceId) {
      input.focus();
    }
  }
}
