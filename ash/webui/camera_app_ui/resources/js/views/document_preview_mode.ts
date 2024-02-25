// Copyright (c) 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists, assertInstanceof} from '../assert.js';
import * as dom from '../dom.js';
import {I18nString} from '../i18n_string.js';
import {Menu} from '../menu.js';
import {getI18nMessage} from '../models/load_time_data.js';
import {MimeType} from '../type.js';
import {instantiateTemplate, loadImage} from '../util.js';

export class DocumentPreviewMode {
  private readonly root: HTMLElement;

  private readonly image: HTMLImageElement;

  private readonly fixPageButton: HTMLButtonElement;

  private readonly addPageButton: HTMLButtonElement;

  private readonly savePdfButton: HTMLButtonElement;

  private readonly menuButton: HTMLButtonElement;

  private fixPageMenuItemLabel: HTMLDivElement|null = null;

  constructor({target, onFix, onAdd, onShare, onCancel, onSave}: {
    target: HTMLElement,
    onFix: () => void,
    onAdd: () => void,
    onShare: () => void,
    onCancel: () => void,
    onSave: (mimeType: MimeType.JPEG|MimeType.PDF) => void,
  }) {
    const fragment = instantiateTemplate('#document-preview-mode');
    this.root = dom.getFrom(fragment, '.document-preview-mode', HTMLElement);
    target.append(this.root);
    this.image = dom.getFrom(this.root, '.image', HTMLImageElement);

    this.fixPageButton = dom.getFrom(
        this.root, 'button[i18n-label=fix_page_button]', HTMLButtonElement);
    this.fixPageButton.addEventListener('click', onFix);
    this.addPageButton = dom.getFrom(
        this.root, 'button[i18n-label=add_new_page_button]', HTMLButtonElement);
    this.addPageButton.addEventListener('click', onAdd);
    dom.getFrom(this.root, 'button[i18n-label=label_share]', HTMLButtonElement)
        .addEventListener('click', onShare);
    dom.getFrom(
           this.root, 'button[i18n-text=cancel_review_button]',
           HTMLButtonElement)
        .addEventListener('click', onCancel);
    dom.getFrom(
           this.root, 'button[i18n-text=label_save_photo_document]',
           HTMLButtonElement)
        .addEventListener('click', () => onSave(MimeType.JPEG));
    this.savePdfButton = dom.getFrom(
        this.root, 'button[i18n-text=label_save_pdf_document]',
        HTMLButtonElement);
    this.savePdfButton.addEventListener('click', () => onSave(MimeType.PDF));

    this.menuButton =
        dom.getFrom(this.root, '#doc-scan-menu-button', HTMLButtonElement);
    const menu = new Menu({
      entryElement: this.menuButton,
      position: {left: 0, top: -8},
      id: 'doc-scan-menu',
      target: assertInstanceof(this.menuButton.parentElement, HTMLElement),
      items: [
        {
          render: (el: HTMLElement) => {
            const {icon, label, container} = makeMenuItemElements();
            icon.setAttribute('name', 'document_review_add_page.svg');
            label.textContent = getI18nMessage(I18nString.ADD_NEW_PAGE_BUTTON);
            container.append(icon, label);
            el.append(container);
          },
          action: onAdd,
        },
        {
          render: (el: HTMLElement) => {
            const {icon, label, container} = makeMenuItemElements();
            icon.setAttribute('name', 'review_share.svg');
            label.textContent = getI18nMessage(I18nString.LABEL_SHARE);
            container.append(icon, label);
            el.append(container);
          },
          action: onShare,
        },
        {
          render: (el: HTMLElement) => {
            const {icon, label, container} = makeMenuItemElements();
            this.fixPageMenuItemLabel = label;
            icon.setAttribute('name', 'document_review_fix_page.svg');
            label.textContent = getI18nMessage(I18nString.FIX_PAGE_BUTTON);
            container.append(icon, label);
            el.append(container);
          },
          action: onFix,
        },
      ],
      anchorOrigin: {vertical: 'top', horizontal: 'center'},
      transformOrigin: {vertical: 'bottom', horizontal: 'left'},
    });
    function makeMenuItemElements() {
      const fragment = instantiateTemplate('#doc-scan-menu-item-content');
      const container =
          dom.getFrom(fragment, '.menu-item-content', HTMLDivElement);
      const icon = dom.getFrom(fragment, '.menu-item-icon', HTMLElement);
      const label = dom.getFrom(fragment, '.menu-item-label', HTMLDivElement);
      return {icon, label, container};
    }
    const observer = new IntersectionObserver((entries) => {
      for (const entry of entries) {
        if (!entry.isIntersecting) {
          menu.close();
        }
      }
    });
    observer.observe(this.menuButton);
  }

  update({src, pageIndex}: {src: string, pageIndex: number}): Promise<void> {
    const label = getI18nMessage(I18nString.FIX_PAGE_BUTTON, pageIndex + 1);
    this.fixPageButton.setAttribute('aria-label', label);
    assertExists(this.fixPageMenuItemLabel).textContent = label;
    return loadImage(this.image, src);
  }

  show(): void {
    this.root.classList.add('show');
  }

  hide(): void {
    this.root.classList.remove('show');
  }

  focusDefaultElement(): void {
    this.savePdfButton.focus();
  }
}
