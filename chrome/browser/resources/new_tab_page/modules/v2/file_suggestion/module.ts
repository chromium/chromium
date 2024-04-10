// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../module_header.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';

import type {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import type {DomRepeat, DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {File} from '../../../file_suggestion.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {FileProxy} from '../../drive/file_module_proxy.js';
import type {InfoDialogElement} from '../../info_dialog.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElementV2} from '../module_header.js';

import {getTemplate} from './module.html.js';

export interface FileSuggestionModuleElement {
  $: {
    fileRepeat: DomRepeat,
    files: HTMLElement,
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
    moduleHeaderElementV2: ModuleHeaderElementV2,
  };
}

/**
 * The File module, which serves as an inside look in to recent activity within
 * a user's Google Drive or Microsoft Sharepoint.
 */
export class FileSuggestionModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-file-module-redesigned';
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

  private getMenuItemGroups_(): MenuItem[][] {
    return [
      [
        {
          action: 'dismiss',
          icon: 'modules:visibility_off',
          text: this.i18n('modulesDriveDismissButtonText'),
        },
        {
          action: 'disable',
          icon: 'modules:block',
          text: this.i18n('modulesDriveDisableButtonTextV2'),
        },
        {
          action: 'info',
          icon: 'modules:info',
          text: this.i18n('moduleInfoButtonTitle'),
        },
      ],
      [
        {
          action: 'customize-module',
          icon: 'modules:tune',
          text: this.i18n('modulesCustomizeButtonText'),
        },
      ],
    ];
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

  private onDismissButtonClick_() {
    FileProxy.getHandler().dismissModule();
    this.dispatchEvent(new CustomEvent('dismiss-module-instance', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage',
            loadTimeData.getString('modulesDriveFilesSentence')),
        restoreCallback: () => FileProxy.getHandler().restoreModule(),
      },
    }));
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

  private onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(FileSuggestionModuleElement.is, FileSuggestionModuleElement);

async function createFileElement(): Promise<FileSuggestionModuleElement|null> {
  const {files} = await FileProxy.getHandler().getFiles();
  if (files.length === 0) {
    return null;
  }
  const element = new FileSuggestionModuleElement();
  element.files = files;
  return element;
}

export const fileSuggestionDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'drive', createFileElement);
