// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Indicates `Read more` button state (listed in upgrade order).
 * @enum {string}
 */
const ReadMoreState = {
  UNKNOWN: 'unknown',
  SHOWN: 'shown',
  HIDDEN: 'hidden',
};

import {afterNextRender, PolymerElement, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_scrollable_behavior.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';

import '../common_styles/oobe_common_styles.css.js';
import '../common_styles/oobe_dialog_host_styles.css.js';
import '../oobe_vars/oobe_custom_vars.css.js';
import '../oobe_vars/oobe_shared_vars.css.js';

/** @polymer */
export class OobeAdaptiveDialog extends PolymerElement {
  static get template() {
    return html`{__html_template__}`;
  }

  static get is() {
    return 'oobe-adaptive-dialog';
  }

  constructor() {
    super();

    this.readMoreState = ReadMoreState.UNKNOWN;
    this.resizeObserver_ = undefined;
  }

  static get properties() {
    return {
      /**
       * If set, prevents lazy instantiation of the dialog.
       */
      noLazy: {
        type: Boolean,
        value: false,
        observer: 'onNoLazyChanged_',
      },

      /**
       * If set, when content overflows, there will be no scrollbar initially.
       * A `Read more` button will be shown and the bottom buttons will be
       * hidden until the `Read More` button is clicked to ensure that the user
       * sees all the content before proceeding. When readMore is set to true,
       * it does not necessarily mean that the `Read more` button will be shown,
       * It will only be shown if the content overflows.
       */
      readMore: {
        type: Boolean,
        value: false,
      },

      /**
       * If set, the width of the dialog header will be wider compared to the
       * the normal dialog in horizontal orientation.
       */
      singleColumn: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * if readMore is set to true and the content overflows contentContainer,
       * showReadMoreButton_ will be set to true to show the `Read more` button
       * and hide the bottom buttons.
       * Once overflown content is shown, either by zooming out, tabbing to
       * hidden content or by clicking the `Read more` button, this property
       * should be set back to false. Don't change it directly, call
       * addReadMoreButton_ and removeReadMoreButton_.
       * @private
       */
      showReadMoreButton_: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * Creates a ResizeObserver and attaches it to the relevant containers
   * to be observed on size changes and scroll position.
   * @private
   */
  addResizeObserver_() {
    if (this.resizeObserver_) {  // Already observing
      return;
    }

    // If `Read more` button is not set, upgrade the state directly to hidden,
    // otherwise, the state will stay unknown until the content is redndered.
    if (this.readMore) {
      this.readMoreState = ReadMoreState.UNKNOWN;
    } else {
      this.readMoreState = ReadMoreState.HIDDEN;
    }

    var scrollContainer = this.shadowRoot.querySelector('#scrollContainer');
    var contentContainer = this.shadowRoot.querySelector('#contentContainer');
    if (!scrollContainer || !contentContainer) {
      return;
    }

    this.resizeObserver_ = new ResizeObserver(() => void this.onResize_());
    this.resizeObserver_.observe(scrollContainer);
    this.resizeObserver_.observe(contentContainer);
  }

  /** @private */
  onResize_() {
    this.maybeUpgradeReadMoreState_(false /* read_more_clicked */);

    // Apply scroll tags when `Read more` button is hidden.
    if (this.readMoreState == ReadMoreState.HIDDEN) {
      this.applyScrollClassTags_();
    }
  }

  /**
   * Applies the class tags to scrollContainer that control the shadows, and
   * updates the `Read more` button state if needed.
   * @private
   */
  applyScrollClassTags_() {
    var el = this.shadowRoot.querySelector('#scrollContainer');
    el.classList.toggle('can-scroll', el.clientHeight < el.scrollHeight);
    el.classList.toggle('is-scrolled', el.scrollTop > 0);
    el.classList.toggle(
        'scrolled-to-bottom',
        el.scrollTop + el.clientHeight >= el.scrollHeight);
  }

  /**
   * Upgrades the `Read More` button State if needed.
   * UNKNOWN -> SHOWN:  If the content overflows the content container.
   * UNKNOWN -> HIDDEN: If the content does not overflow the content container.
   * SHOWN   -> HIDDEN: If `Read more` is clicked, the content stopped
   * overflowing the content container or the container is scrolled.
   *
   * @param {boolean} read_more_clicked Whether the `Read more` button clicked
   *     or not.
   * @private
   */
  maybeUpgradeReadMoreState_(read_more_clicked) {
    // HIDDEN is the final state. We cannot move from HIDDEN state to SHOWN or
    // UNKNOWN state.
    if (this.readMoreState == ReadMoreState.HIDDEN) {
      return;
    }

    if (read_more_clicked) {
      this.readMoreState = ReadMoreState.HIDDEN;
      this.removeReadMoreButton_();
      return;
    }
    var content = this.shadowRoot.querySelector('#contentContainer');
    if (this.readMoreState == ReadMoreState.UNKNOWN) {
      if (content.clientHeight < content.scrollHeight) {
        this.readMoreState = ReadMoreState.SHOWN;
        this.addReadMoreButton_();
      } else {
        this.readMoreState = ReadMoreState.HIDDEN;
      }
    } else if (this.readMoreState == ReadMoreState.SHOWN) {
      if (content.clientHeight >= content.scrollHeight ||
          content.scrollTop > 0) {
        this.readMoreState = ReadMoreState.HIDDEN;
        this.removeReadMoreButton_();
      }
    }
  }

  focus() {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  }

  onBeforeShow() {
    this.shadowRoot.querySelector('#lazy').get();
    this.addResizeObserver_();
  }

  /**
   * Scroll to the bottom of footer container.
   */
  scrollToBottom() {
    var el = this.shadowRoot.querySelector('#scrollContainer');
    el.scrollTop = el.scrollHeight;
  }

  /**
   * @private
   * Focuses the element. As cr-input uses focusInput() instead of focus() due
   * to bug, we have to handle this separately.
   * TODO(crbug.com/882612): Replace this with focus() in show().
   */
  focusElement_(element) {
    if (element.focusInput) {
      element.focusInput();
      return;
    }
    element.focus();
  }

  /** @private */
  focusOnShow_() {
    var focusedElements = this.getElementsByClassName('focus-on-show');
    var focused = false;
    for (var i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden) {
        continue;
      }

      focused = true;
      afterNextRender(this, () => this.focusElement_(focusedElements[i]));
      break;
    }
    if (!focused && focusedElements.length > 0) {
      afterNextRender(this, () => this.focusElement_(focusedElements[0]));
    }
  }

  /**
   * This is called when this dialog is shown.
   */
  show() {
    this.focusOnShow_();
    this.dispatchEvent(
        new CustomEvent('show-dialog', {bubbles: true, composed: true}));
  }

  /** @private */
  onNoLazyChanged_() {
    if (this.noLazy) {
      this.shadowRoot.querySelector('#lazy').get();
    }
  }

  /** @private */
  addReadMoreButton_() {
    var contentContainer = this.shadowRoot.querySelector('#contentContainer');
    contentContainer.setAttribute('read-more-content', true);
    this.showReadMoreButton_ = true;

    afterNextRender(this, () => {
      var readMoreButton = this.shadowRoot.querySelector('#readMoreButton');
      this.focusElement_(readMoreButton);
    });

    // Once a tab reaches an element outside of the visible area, call
    // maybeUpgradeReadMoreState_ to apply changes.
    contentContainer.addEventListener('keyup', (event) => {
      if (!this.showReadMoreButton_) {
        return;
      }
      if (event.which === 9) {
        if (contentContainer.scrollTop > 0) {
          this.maybeUpgradeReadMoreState_(true /* read_more_clicked */);
        }
      }
    });
  }

  /** @private */
  removeReadMoreButton_() {
    var contentContainer = this.shadowRoot.querySelector('#contentContainer');
    contentContainer.removeAttribute('read-more-content');
    this.showReadMoreButton_ = false;

    // If `read more` button is focused after it was removed, move focus to the
    // 'focus-on-show' element.
    var readMoreButton = this.shadowRoot.querySelector('#readMoreButton');
    if (this.shadowRoot.activeElement == readMoreButton) {
      this.focusOnShow_();
    }

    this.scrollToBottom();
  }

  /** @private */
  onReadMoreClick_() {
    this.maybeUpgradeReadMoreState_(true /* read_more_clicked */);
  }
}

customElements.define(OobeAdaptiveDialog.is, OobeAdaptiveDialog);
