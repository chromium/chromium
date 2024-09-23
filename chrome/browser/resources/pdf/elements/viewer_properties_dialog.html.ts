// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ViewerPropertiesDialogElement} from './viewer_properties_dialog.js';

export function getHtml(this: ViewerPropertiesDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="dialog" show-on-attach>
  <div slot="title">$i18n{propertiesDialogTitle}</div>
  <div slot="body">
    <table>
      <tr>
        <td class="name">$i18n{propertiesFileName}</td>
        <td class="value" id="file-name">${this.fileName}</td>
      </tr>
      <tr class="break">
        <td class="name">$i18n{propertiesFileSize}</td>
        <td class="value" id="file-size">${this.documentMetadata.fileSize}</td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesTitle}</td>
        <td class="value" id="title">
          ${this.getOrPlaceholder_(this.documentMetadata.title)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesAuthor}</td>
        <td class="value" id="author">
          ${this.getOrPlaceholder_(this.documentMetadata.author)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesSubject}</td>
        <td class="value" id="subject">
          ${this.getOrPlaceholder_(this.documentMetadata.subject)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesKeywords}</td>
        <td class="value" id="keywords">
          ${this.getOrPlaceholder_(this.documentMetadata.keywords)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesCreated}</td>
        <td class="value" id="created">
          ${this.getOrPlaceholder_(this.documentMetadata.creationDate)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesModified}</td>
        <td class="value" id="modified">
          ${this.getOrPlaceholder_(this.documentMetadata.modDate)}
        </td>
      </tr>
      <tr class="break">
        <td class="name">$i18n{propertiesApplication}</td>
        <td class="value" id="application">
          ${this.getOrPlaceholder_(this.documentMetadata.creator)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesPdfProducer}</td>
        <td class="value" id="pdf-producer">
          ${this.getOrPlaceholder_(this.documentMetadata.producer)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesPdfVersion}</td>
        <td class="value" id="pdf-version">
          ${this.getOrPlaceholder_(this.documentMetadata.version)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesPageCount}</td>
        <td class="value" id="page-count">${this.pageCount}</td>
      </tr>
      <tr class="break">
        <td class="name">$i18n{propertiesPageSize}</td>
        <td class="value" id="page-size">
          ${this.getOrPlaceholder_(this.documentMetadata.pageSize)}
        </td>
      </tr>
      <tr>
        <td class="name">$i18n{propertiesFastWebView}</td>
        <td class="value" id="fast-web-view">
          ${this.getFastWebViewValue_('$i18nPolymer{propertiesFastWebViewYes}',
              '$i18nPolymer{propertiesFastWebViewNo}',
              this.documentMetadata.linearized)}
        </td>
      </tr>
    </table>
  </div>
  <div slot="button-container">
    <cr-button id="close" class="action-button" @click="${this.onClickClose_}">
      $i18n{propertiesDialogClose}
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
