// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../module_header.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {DomRepeat, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {File} from '../../drive.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../i18n_setup.js';
import {DriveProxy} from '../drive/drive_module_proxy.js';
import {InfoDialogElement} from '../info_dialog.js';
import {ModuleDescriptor} from '../module_descriptor.js';

import {getTemplate} from './module.html.js';

export interface DriveModuleElement {
  $: {
    fileRepeat: DomRepeat,
    files: HTMLElement,
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

/**
 * The Drive module, which serves as an inside look in to recent activity within
 * a user's Google Drive.
 */
export class DriveModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-drive-module-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      files: Array,
    };
  }

  files: File[];

  private getImageSrc_(file: File): string {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableModuleToastMessage',
            loadTimeData.getString('modulesDriveSentence2')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onFileClick_(e: DomRepeatEvent<File>) {
    const clickFileEvent = new Event('usage', {composed: true});
    this.dispatchEvent(clickFileEvent);
    chrome.metricsPrivate.recordSmallCount(
        'NewTabPage.Drive.FileClick', e.model.index);
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }
}

customElements.define(DriveModuleElement.is, DriveModuleElement);

async function createDriveElement(): Promise<DriveModuleElement> {
  const {files} = await DriveProxy.getHandler().getFiles();
  const element = new DriveModuleElement();
  element.files = files.slice(0, 2);
  return element;
}

export const driveDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'drive', createDriveElement);
