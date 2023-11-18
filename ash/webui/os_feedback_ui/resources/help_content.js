// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './help_resources_icons.html.js';
import './strings.m.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icons.css.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SearchResult} from './feedback_types.js';
import {getTemplate} from './help_content.html.js';
import {HelpContent, HelpContentType} from './os_feedback_ui.mojom-webui.js';

/**
 * The host of trusted parent page.
 * @type {string}
 */
export const OS_FEEDBACK_TRUSTED_ORIGIN = 'chrome://os-feedback';

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

/**
 * @constructor
 * @implements {I18nBehaviorInterface}
 * @extends {PolymerElement}
 */
const HelpContentElementBase = mixinBehaviors([I18nBehavior], PolymerElement);

/**
 * @polymer
 */
export class HelpContentElement extends HelpContentElementBase {
  static get is() {
    return 'help-content';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      searchResult: {type: SearchResult},
      isDarkModeEnabled_: {type: Boolean},
      isJellyEnabled_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isJellyEnabledForOsFeedback');
        },
      },
      isOnline_: {type: Boolean},
    };
  }

  constructor() {
    super();

    /**
     * @type {!SearchResult}
     */
    this.searchResult = {
      contentList: [],
      isQueryEmpty: true,
      isPopularContent: true,
    };

    /** @type {boolean} */
    this.isDarkModeEnabled_ = false;

    /** @type {boolean} */
    this.isOnline_ = navigator.onLine;
  }

  /** @override */
  ready() {
    super.ready();

    window.addEventListener('online', () => {
      this.isOnline_ = true;
    });

    window.addEventListener('offline', () => {
      this.isOnline_ = false;
    });

    // Send the height of the content to the parent window so that it can set
    // the height of the iframe correctly.
    const helpContent = this.shadowRoot.querySelector('#helpContentContainer');
    const resizeObserver = new ResizeObserver(() => {
      window.parent.postMessage(
          {iframeHeight: helpContent.scrollHeight}, OS_FEEDBACK_TRUSTED_ORIGIN);
    });
    if (helpContent) {
      resizeObserver.observe(helpContent);
    }
  }

  notifyParent() {}

  /**
   * Compute the label to use.
   * @return {string}
   * @protected
   */
  getLabel_() {
    if (!this.isOnline_) {
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

  /**
   * Returns true if there are suggested help content displayed.
   * @return {boolean}
   * @protected
   */
  hasSuggestedHelpContent_() {
    return (this.isOnline_ && !this.searchResult.isPopularContent);
  }

  /**
   * When there isn't available help content to display, display such a message
   * with an image.
   * @return {boolean}
   * @protected
   */
  showHelpContentNotAvailableMsg_() {
    return this.searchResult.contentList.length === 0;
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

  /**
   * Gets the relative source path to the "help content is offline"
   * illustration.
   * @return {string}
   * @protected
   */
  getOfflineIllustrationSrc_() {
    if (this.isDarkModeEnabled_) {
      return 'illustrations/network_unavailable_darkmode.svg';
    } else {
      return 'illustrations/network_unavailable_lightmode.svg';
    }
  }

  /**
   * Gets the relative source path to the "help content isn't available"
   * illustration.
   * @return {string}
   * @protected
   */
  getContentNotAvailableIllustrationSrc_() {
    if (this.isDarkModeEnabled_) {
      return 'illustrations/load_content_error_darkmode.svg';
    } else {
      return 'illustrations/load_content_error_lightmode.svg';
    }
  }

  /**
   * @param {!Event} e
   * @protected
   */
  handleHelpContentClicked_(e) {
    e.stopPropagation();
    window.parent.postMessage(
        {
          id: 'help-content-clicked',
        },
        OS_FEEDBACK_TRUSTED_ORIGIN);
  }
}

customElements.define(HelpContentElement.is, HelpContentElement);
