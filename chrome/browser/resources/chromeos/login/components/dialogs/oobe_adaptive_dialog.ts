// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_scrollable_behavior.js';
import '../common_styles/oobe_common_styles.css.js';
import '../common_styles/oobe_dialog_host_styles.css.js';
import '../oobe_vars/oobe_custom_vars.css.js';
import '../oobe_vars/oobe_shared_vars.css.js';

import {CrButtonElement} from '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrLazyRenderElement} from '//resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_adaptive_dialog.html.js';

/**
 * Indicates `Read more` button state (listed in upgrade order).
 */
enum ReadMoreState {
  UNKNOWN = 'unknown',
  SHOWN = 'shown',
  HIDDEN = 'hidden',
}

export class OobeAdaptiveDialog extends PolymerElement {
  static get is() {
    return 'oobe-adaptive-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * If set, prevents lazy instantiation of the dialog.
       */
      noLazy: {
        type: Boolean,
        value: false,
        observer: 'onNoLazyChanged',
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
       * the normal dialog in landscape orientation.
       */
      singleColumn: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * if readMore is set to true and the content overflows contentContainer,
       * showReadMoreButton will be set to true to show the `Read more` button
       * and hide the bottom buttons.
       * Once overflown content is shown, either by zooming out, tabbing to
       * hidden content or by clicking the `Read more` button, this property
       * should be set back to false. Don't change it directly, call
       * addReadMoreButton and removeReadMoreButton.
       */
      showReadMoreButton: {
        type: Boolean,
        value: false,
      },
    };
  }

  private noLazy: boolean;
  private readMore: boolean;
  private singleColumn: boolean;
  private showReadMoreButton: boolean;
  private resizeObserver?: ResizeObserver;
  private readMoreState: ReadMoreState;

  constructor() {
    super();

    this.readMoreState = ReadMoreState.UNKNOWN;
    this.resizeObserver = undefined;
  }

  private getLazyRender(): CrLazyRenderElement<HTMLElement> {
    const lazyRender = this.shadowRoot?.querySelector('#lazy');
    assert(lazyRender instanceof CrLazyRenderElement);
    return lazyRender;
  }

  private getReadMoreButton(): CrButtonElement|null {
    const readMoreButton = this.shadowRoot?.querySelector('#readMoreButton');
    return readMoreButton instanceof CrButtonElement ? readMoreButton : null;
  }

  private getScrollContainer(): HTMLDivElement|null {
    const scrollContainer = this.shadowRoot?.querySelector('#scrollContainer');
    return scrollContainer instanceof HTMLDivElement ? scrollContainer : null;
  }

  private getContentContainer(): HTMLDivElement|null {
    const contentContainer =
        this.shadowRoot?.querySelector('#contentContainer');
    return contentContainer instanceof HTMLDivElement ? contentContainer : null;
  }

  /**
   * Creates a ResizeObserver and attaches it to the relevant containers
   * to be observed on size changes and scroll position.
   */
  private addResizeObserver(): void {
    if (this.resizeObserver) {
      return;
    }

    // If `Read more` button is not set, upgrade the state directly to hidden,
    // otherwise, the state will stay unknown until the content is redndered.
    if (this.readMore) {
      this.readMoreState = ReadMoreState.UNKNOWN;
    } else {
      this.readMoreState = ReadMoreState.HIDDEN;
    }

    const scrollContainer = this.getScrollContainer();
    const contentContainer = this.getContentContainer();
    if (!scrollContainer || !contentContainer) {
      return;
    }

    scrollContainer.addEventListener(
      'scroll', this.applyScrollClassTags.bind(this));
    this.resizeObserver = new ResizeObserver(() => void this.onResize());
    this.resizeObserver.observe(scrollContainer);
    this.resizeObserver.observe(contentContainer);
  }

  private onResize(): void {
    this.maybeUpgradeReadMoreState(false /* readMoreClicked */);

    // Apply scroll tags when `Read more` button is hidden.
    if (this.readMoreState === ReadMoreState.HIDDEN) {
      this.applyScrollClassTags();
    }
  }

  /**
   * Applies the class tags to scrollContainer that control the shadows, and
   * updates the `Read more` button state if needed.
   */
  private applyScrollClassTags(): void {
    const scrollContainer = this.getScrollContainer();
    assert(scrollContainer instanceof HTMLDivElement);
    scrollContainer.classList.toggle(
        'can-scroll',
        scrollContainer.clientHeight < scrollContainer.scrollHeight);
    scrollContainer.classList.toggle(
        'is-scrolled', scrollContainer.scrollTop > 0);
    scrollContainer.classList.toggle(
        'scrolled-to-bottom',
        scrollContainer.scrollTop + scrollContainer.clientHeight >=
            scrollContainer.scrollHeight);
  }

  /**
   * Upgrades the `Read More` button State if needed.
   * UNKNOWN -> SHOWN:  If the content overflows the content container.
   * UNKNOWN -> HIDDEN: If the content does not overflow the content container.
   * SHOWN   -> HIDDEN: If `Read more` is clicked, the content stopped
   * overflowing the content container or the container is scrolled.
   *
   * @param readMoreClicked Whether the `Read more` button clicked
   *     or not.
   */
  private maybeUpgradeReadMoreState(readMoreClicked: boolean): void {
    // HIDDEN is the final state. We cannot move from HIDDEN state to SHOWN or
    // UNKNOWN state.
    if (this.readMoreState === ReadMoreState.HIDDEN) {
      return;
    }

    if (readMoreClicked) {
      this.readMoreState = ReadMoreState.HIDDEN;
      this.removeReadMoreButton();
      return;
    }
    const content = this.getContentContainer();
    assert(content instanceof HTMLDivElement);
    if (this.readMoreState === ReadMoreState.UNKNOWN) {
      if (content.clientHeight < content.scrollHeight) {
        this.readMoreState = ReadMoreState.SHOWN;
        this.addReadMoreButton();
      } else {
        this.readMoreState = ReadMoreState.HIDDEN;
      }
    } else if (this.readMoreState === ReadMoreState.SHOWN) {
      if (content.clientHeight >= content.scrollHeight ||
          content.scrollTop > 0) {
        this.readMoreState = ReadMoreState.HIDDEN;
        this.removeReadMoreButton();
      }
    }
  }

  override focus(): void {
    /* When Network Selection Dialog is shown because user pressed "Back"
       button on EULA screen, display_manager does not inform this dialog that
       it is shown. It ouly focuses this dialog.
       So this emulates show().
       TODO (alemate): fix this once event flow is updated.
    */
    this.show();
  }

  onBeforeShow(): void {
    this.getLazyRender().get();
    this.addResizeObserver();
  }

  /**
   * Scroll to the bottom of footer container.
   */
  private scrollToBottom(): void {
    const scrollContainer = this.getScrollContainer();
    assert(scrollContainer instanceof HTMLDivElement);
    scrollContainer.scrollTop = scrollContainer.scrollHeight;
  }

  private focusOnShow(): void {
    const focusedElements =
        this.querySelectorAll<HTMLElement>('.focus-on-show');
    let focused = false;
    for (let i = 0; i < focusedElements.length; ++i) {
      if (focusedElements[i].hidden) {
        continue;
      }

      focused = true;
      afterNextRender(this, () => focusedElements[i].focus());
      break;
    }
    if (!focused && focusedElements.length > 0) {
      afterNextRender(this, () => focusedElements[0].focus());
    }
  }

  /**
   * This is called when this dialog is shown.
   */
  show(): void {
    this.focusOnShow();
    this.dispatchEvent(
        new CustomEvent('show-dialog', {bubbles: true, composed: true}));
  }

  private onNoLazyChanged(): void {
    if (this.noLazy) {
      this.getLazyRender().get();
    }
  }

  private addReadMoreButton(): void {
    const contentContainer = this.getContentContainer();
    assert(contentContainer instanceof HTMLDivElement);
    contentContainer.toggleAttribute('read-more-content', true);
    this.showReadMoreButton = true;

    afterNextRender(this, () => {
      const readMoreButton = this.getReadMoreButton();
      assert(readMoreButton);
      readMoreButton.focus();
    });

    // Once a tab reaches an element outside of the visible area, call
    // maybeUpgradeReadMoreState to apply changes.
    contentContainer.addEventListener('keyup', (event: KeyboardEvent) => {
      if (!this.showReadMoreButton) {
        return;
      }
      if (event.which === 9) {
        if (contentContainer.scrollTop > 0) {
          this.maybeUpgradeReadMoreState(true /* readMoreClicked */);
        }
      }
    });
  }

  private removeReadMoreButton(): void {
    const contentContainer = this.getContentContainer();
    assert(contentContainer instanceof HTMLDivElement);
    contentContainer.removeAttribute('read-more-content');
    this.showReadMoreButton = false;

    // If `read more` button is focused after it was removed, move focus to the
    // 'focus-on-show' element.
    if (this.shadowRoot?.activeElement === this.getReadMoreButton()) {
      this.focusOnShow();
    }

    this.scrollToBottom();
  }

  private onReadMoreClick(): void {
    this.maybeUpgradeReadMoreState(true /* readMoreClicked */);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeAdaptiveDialog.is]: OobeAdaptiveDialog;
  }
}

customElements.define(OobeAdaptiveDialog.is, OobeAdaptiveDialog);
