// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scan_done_section.html.js';
import {ScanCompleteAction} from './scanning_app_types.js';
import {ScanningBrowserProxyImpl} from './scanning_browser_proxy.js';

/**
 * @fileoverview
 * 'scan-done-section' shows the post-scan user options.
 */

const ScanDoneSectionElementBase = I18nMixin(PolymerElement);

export class ScanDoneSectionElement extends ScanDoneSectionElementBase {
  static get is() {
    return 'scan-done-section' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      numFilesSaved: {
        type: Number,
        observer: ScanDoneSectionElement.prototype.numFilesSavedChanged,
      },

      scannedFilePaths: Array,

      selectedFileType: String,

      selectedFolder: String,

      fileSavedTextContent: String,

      editButtonLabel: String,
    };
  }

  static get observers() {
    return ['setFileSavedTextContent(numFilesSaved, selectedFolder)'];
  }

  numFilesSaved: number;
  scannedFilePaths: FilePath[];
  selectedFileType: string;
  selectedFolder: string;
  fileSavedTextContent: TrustedHTML|string;
  editButtonLabel: string;
  // ScanningBrowserProxy is initialized when scanning_app.js is created.
  private browserProxy = ScanningBrowserProxyImpl.getInstance();

  private onDoneClick(): void {
    this.browserProxy.recordScanCompleteAction(
        ScanCompleteAction.DONE_BUTTON_CLICKED);
    this.dispatchEvent(
        new CustomEvent('done-click', {bubbles: true, composed: true}));
  }

  private onShowFileInLocationClick(): void {
    assert(this.scannedFilePaths.length !== 0);

    this.browserProxy.recordScanCompleteAction(
        ScanCompleteAction.FILES_APP_OPENED);
    this.browserProxy
        .showFileInLocation(this.scannedFilePaths.slice(-1)[0].path)
        .then((successful: boolean): void => {
          if (!successful) {
            this.dispatchEvent(new CustomEvent(
                'file-not-found', {bubbles: true, composed: true}));
          }
        });
  }

  private setFileSavedTextContent(): void {
    this.browserProxy.getPluralString('fileSavedText', this.numFilesSaved)
        .then((pluralString: string): void => {
          const fileSavedTextContent =
              this.getAriaLabelledContent(loadTimeData.substituteString(
                  pluralString.toString(), this.selectedFolder));
          this.fileSavedTextContent = sanitizeInnerHtml(
              fileSavedTextContent,
              {attrs: ['id', 'aria-hidden', 'aria-labelledby']});
          const linkElement =
              strictQuery('#folderLink', this.shadowRoot, HTMLAnchorElement);
          linkElement.setAttribute('href', '#');
          linkElement.addEventListener(
              'click', () => this.onShowFileInLocationClick());
        });
  }

  /**
   * Takes a localized string that contains exactly one anchor tag and labels
   * the string contained within the anchor tag with the entire localized
   * string. The string should not be bound by element tags. The string should
   * not contain any elements other than the single anchor tagged element that
   * will be aria-labelledby the entire string.
   */
  private getAriaLabelledContent(localizedString: string): string {
    const tempEl = document.createElement('div');
    tempEl.innerHTML = sanitizeInnerHtml(localizedString, {attrs: ['id']});

    const ariaLabelledByIds: string[] = [];
    tempEl.childNodes.forEach((node: ChildNode, index: number): void => {
      // Text nodes should be aria-hidden and associated with an element id
      // that the anchor element can be aria-labelledby.
      if (node.nodeType == Node.TEXT_NODE) {
        const spanNode: HTMLSpanElement = document.createElement('span');
        spanNode.textContent = node.textContent;
        spanNode.id = `id${index}`;
        ariaLabelledByIds.push(spanNode.id);
        spanNode.setAttribute('aria-hidden', 'true');
        node.replaceWith(spanNode);
        return;
      }

      // The single element node with anchor tags should also be aria-labelledby
      // itself in-order with respect to the entire string.
      if (node.nodeType == Node.ELEMENT_NODE && node.nodeName == 'A') {
        ariaLabelledByIds.push((node as HTMLAnchorElement).id);
        return;
      }
    });

    const anchorTags = tempEl.getElementsByTagName('a');
    anchorTags[0].setAttribute('aria-labelledby', ariaLabelledByIds.join(' '));

    return tempEl.innerHTML;
  }

  private onOpenMediaAppClick(): void {
    assert(this.scannedFilePaths.length !== 0);

    this.browserProxy.recordScanCompleteAction(
        ScanCompleteAction.MEDIA_APP_OPENED);
    this.browserProxy.openFilesInMediaApp(
        this.scannedFilePaths.map(filePath => filePath.path));
  }

  private numFilesSavedChanged(): void {
    this.browserProxy.getPluralString('editButtonLabel', this.numFilesSaved)
        .then((pluralString: string): void => {
          this.editButtonLabel = pluralString;
        });
  }
}

declare global {
  interface HTMLElementEventMap {
    'done-click': CustomEvent<void>;
    'file-not-found': CustomEvent<void>;
  }

  interface HTMLElementTagNameMap {
    [ScanDoneSectionElement.is]: ScanDoneSectionElement;
  }
}

customElements.define(ScanDoneSectionElement.is, ScanDoneSectionElement);
