// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../dom.js';
import {MimeType} from '../type.js';
import {instantiateTemplate, loadImage} from '../util.js';

export class DocumentPreviewMode {
  private readonly templateSelector = '#document-preview-mode';

  private readonly root: HTMLElement;

  private readonly image: HTMLImageElement;

  /**
   * The target element to append the root element of fix mode.
   */
  private readonly target: HTMLElement;

  constructor({target, onFix, onAdd, onShare, onCancel, onSave}: {
    target: HTMLElement,
    onFix: () => void,
    onAdd: () => void,
    onShare: () => void,
    onCancel: () => void,
    onSave: (mimeType: MimeType.JPEG|MimeType.PDF) => void,
  }) {
    this.target = target;
    const fragment = instantiateTemplate(this.templateSelector);
    this.root = dom.getFrom(fragment, '.document-preview-mode', HTMLElement);
    this.image = dom.getFrom(this.root, '.image', HTMLImageElement);

    dom.getFrom(
           this.root, 'button[i18n-aria=label_fix_document]', HTMLButtonElement)
        .addEventListener('click', onFix);
    dom.getFrom(
           this.root, 'button[i18n-aria=add_new_page_button]',
           HTMLButtonElement)
        .addEventListener('click', onAdd);
    dom.getFrom(this.root, 'button[i18n-aria=label_share]', HTMLButtonElement)
        .addEventListener('click', onShare);
    dom.getFrom(
           this.root, 'button[i18n-text=cancel_review_button]',
           HTMLButtonElement)
        .addEventListener('click', onCancel);
    dom.getFrom(
           this.root, 'button[i18n-text=label_save_pdf_document]',
           HTMLButtonElement)
        .addEventListener('click', () => onSave(MimeType.PDF));
    dom.getFrom(
           this.root, 'button[i18n-text=label_save_photo_document]',
           HTMLButtonElement)
        .addEventListener('click', () => onSave(MimeType.JPEG));
  }

  update({src}: {src: string}): Promise<void> {
    return loadImage(this.image, src);
  }

  show(): void {
    this.target.append(this.root);
  }

  hide(): void {
    this.root.remove();
  }
}
