// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.html.js';
import './strings.m.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';
import '//resources/ash/common/cr_elements/cr_icons.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from '//resources/ash/common/cr_elements/i18n_mixin.js';
import {strictQuery} from '//resources/ash/common/typescript_utils/strict_query.js';
import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchResult} from './feedback_types.js';
import {getTemplate} from './help_content.html.js';
import {HelpContent, HelpContentType} from './os_feedback_ui.mojom-webui.js';

/** The host of trusted parent page. */
export const OS_FEEDBACK_TRUSTED_ORIGIN = 'chrome://os-feedback';

const ICON_NAME_FOR_ARTICLE = 'content-type:article';

const ICON_NAME_FOR_FORUM = 'content-type:forum';

/**
 * @fileoverview
 * 'help-content' displays list of help contents.
 */

const HelpContentElementBase = I18nMixin(PolymerElement);

export class HelpContentElement extends HelpContentElementBase {
  static get is() {
    return 'help-content' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchResult: {
        type: Object,
      },
      isOnline: {type: Boolean},
    };
  }

  searchResult: SearchResult;
  private isOnline = navigator.onLine;

  constructor() {
    super();
    this.searchResult = {
      contentList: [],
      isQueryEmpty: true,
      isPopularContent: true,
    };
  }

  override ready() {
    super.ready();

    window.addEventListener('online', () => {
      this.isOnline = true;
    });

    window.addEventListener('offline', () => {
      this.isOnline = false;
    });

    // Send the height of the content to the parent window so that it can set
    // the height of the iframe correctly.
    const helpContent =
        strictQuery('#helpContentContainer', this.shadowRoot, HTMLElement);
    const resizeObserver = new ResizeObserver(() => {
      window.parent.postMessage(
          {iframeHeight: helpContent.scrollHeight}, OS_FEEDBACK_TRUSTED_ORIGIN);
    });
    if (helpContent) {
      resizeObserver.observe(helpContent);
    }
  }

  notifyParent() {}

  /** Compute the label to use. */
  private getLabel(): string {
    if (!this.isOnline) {
      return this.i18n('popularHelpContent');
    }
    if (!this.searchResult.isPopularContent) {
      return this.i18n('suggestedHelpContent');
    }
    if (this.searchResult.isQueryEmpty) {
      return this.i18n('popularHelpContent');
    }
    return this.i18n('noMatchedResults');
  }

  /** Returns true if there are suggested help content displayed. */
  private hasSuggestedHelpContent(): boolean {
    return (this.isOnline && !this.searchResult.isPopularContent);
  }

  /**
   * When there isn't available help content to display, display such a message
   * with an image.
   */
  private showHelpContentNotAvailableMsg(): boolean {
    return this.searchResult.contentList.length === 0;
  }

  /** Find the icon name to be used for a help content type. */
  private getIcon(contentType: HelpContentType): string {
    switch (contentType) {
      case HelpContentType.kForum:
        return ICON_NAME_FOR_FORUM;
      case HelpContentType.kArticle:
        return ICON_NAME_FOR_ARTICLE;
      case HelpContentType.kUnknown:
        return ICON_NAME_FOR_ARTICLE;
      default:
        return ICON_NAME_FOR_ARTICLE;
    }
  }

  /** Extract the url string from help content. */
  private getUrl(helpContent: HelpContent): string {
    return helpContent.url.url;
  }

  /** Extract the title as JS string from help content. */
  private getTitle(helpContent: HelpContent): string {
    return mojoString16ToString(helpContent.title);
  }

  private handleHelpContentClicked(e: Event): void {
    e.stopPropagation();
    window.parent.postMessage(
        {
          id: 'help-content-clicked',
        },
        OS_FEEDBACK_TRUSTED_ORIGIN);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HelpContentElement.is]: HelpContentElement;
  }
}

customElements.define(HelpContentElement.is, HelpContentElement);
