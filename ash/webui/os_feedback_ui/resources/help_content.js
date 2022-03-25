// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {mojoString16ToString} from '//resources/ash/common/mojo_utils.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {HelpContent, HelpContentList, HelpContentType, SearchResult} from './feedback_types.js';

/**
 * @const {string}
 */
const ICON_NAME_FOR_ARTICLE = 'content-type:article';

/**
 * @const {string}
 */
const ICON_NAME_FOR_FORUM = 'content-type:forum';

/**
 * @fileoverview
 * 'help-content' displays list of help contents.
 */
export class HelpContentElement extends PolymerElement {
  static get is() {
    return 'help-content';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {searchResult: {type: SearchResult}};
  }

  constructor() {
    super();

    /**
     * @type {!SearchResult}
     */
    this.searchResult = {
      contentList: [],
      isQueryEmpty: true,
      isPopularContent: true
    };
  }

  /**
   * Compute the label to use.
   * @param {!SearchResult} searchResult
   * @returns {string}
   * @protected
   */
  getLabel_(searchResult) {
    // TODO(xiangdongkong): Use localized strings.
    if (!searchResult.isPopularContent) {
      return 'Suggested help content';
    }
    if (searchResult.isQueryEmpty) {
      return 'Popular help content';
    }
    return 'No matched results, see popular help content';
  }

  /**
   * Find the icon name to be used for a help content type.
   * @param {!HelpContentType} contentType
   * @return {string}
   * @protected
   */
  getIcon_(contentType) {
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

  /**
   * Extract the url string from help content.
   * @param {!HelpContent} helpContent
   * @return {string}
   * @protected
   */
  getUrl_(helpContent) {
    return helpContent.url.url;
  }

  /**
   * Extract the title as JS string from help content.
   * @param {!HelpContent} helpContent
   * @return {string}
   * @protected
   */
  getTitle_(helpContent) {
    return mojoString16ToString(helpContent.title);
  }
}

customElements.define(HelpContentElement.is, HelpContentElement);
