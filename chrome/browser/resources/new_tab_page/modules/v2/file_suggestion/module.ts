// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../info_dialog.js';
import '../../module_header.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {File} from '../../../file_suggestion.mojom-webui.js';
import {I18nMixinLit, loadTimeData} from '../../../i18n_setup.js';
import {FileProxy} from '../../drive/file_module_proxy.js';
import {ModuleDescriptor} from '../../module_descriptor.js';
import type {MenuItem, ModuleHeaderElement} from '../module_header.js';

import {getCss} from './module.css.js';
import {getHtml} from './module.html.js';

export interface ModuleElement {
  $: {
    files: HTMLElement,
    moduleHeaderElementV2: ModuleHeaderElement,
  };
}

const ModuleElementBase = I18nMixinLit(CrLitElement);

/**
 * The File module, which serves as an inside look in to recent activity within
 * a user's Google Drive or Microsoft Sharepoint.
 */
export class ModuleElement extends ModuleElementBase {
  static get is() {
    return 'ntp-file-module-redesigned';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      files: {type: Array},
      showInfoDialog_: {type: Boolean},
    };
  }

  files: File[] = [];
  protected showInfoDialog_: boolean = false;

  protected getImageSrc_(file: File): string {
    return 'https://drive-thirdparty.googleusercontent.com/32/type/' +
        file.mimeType;
  }

  protected getMenuItemGroups_(): MenuItem[][] {
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

  protected onDisableButtonClick_() {
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

  protected onDismissButtonClick_() {
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

  protected onFileClick_(e: Event) {
    const clickFileEvent = new Event('usage', {composed: true, bubbles: true});
    this.dispatchEvent(clickFileEvent);
    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    chrome.metricsPrivate.recordSmallCount('NewTabPage.Drive.FileClick', index);
  }

  protected onInfoButtonClick_() {
    this.showInfoDialog_ = true;
  }

  protected onInfoDialogClose_() {
    this.showInfoDialog_ = false;
  }

  protected onMenuButtonClick_(e: Event) {
    this.$.moduleHeaderElementV2.showAt(e);
  }
}

customElements.define(ModuleElement.is, ModuleElement);

async function createFileElement(): Promise<ModuleElement|null> {
  const {files} = await FileProxy.getHandler().getFiles();
  if (files.length === 0) {
    return null;
  }
  const element = new ModuleElement();
  element.files = files;
  return element;
}

export const fileSuggestionDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id*/ 'drive', createFileElement);
