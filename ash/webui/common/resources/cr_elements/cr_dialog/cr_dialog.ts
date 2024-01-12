// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-dialog' is a component for showing a modal dialog. If the
 * dialog is closed via close(), a 'close' event is fired. If the dialog is
 * canceled via cancel(), a 'cancel' event is fired followed by a 'close' event.
 *
 * Additionally clients can get a reference to the internal native <dialog> via
 * calling getNative() and inspecting the |returnValue| property inside
 * the 'close' event listener to determine whether it was canceled or just
 * closed, where a truthy value means success, and a falsy value means it was
 * canceled.
 *
 * Note that <cr-dialog> wrapper itself always has 0x0 dimensions, and
 * specifying width/height on <cr-dialog> directly will have no effect on the
 * internal native <dialog>. Instead use cr-dialog::part(dialog) to specify
 * width/height (as well as other available mixins to style other parts of the
 * dialog contents).
 *
 * Forked from ui/webui/resources/cr_elements/cr_dialog/cr_dialog.ts
 */
import '../cr_icon_button/cr_icon_button.js';
import '../cr_icons.css.js';
import '../cr_hidden_style.css.js';
import '../cr_shared_vars.css.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrContainerShadowMixin} from '../cr_container_shadow_mixin.js';
import {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';
import {CrInputElement} from '../cr_input/cr_input.js';

import {getTemplate} from './cr_dialog.html.js';

const CrDialogElementBase = CrContainerShadowMixin(PolymerElement);

export interface CrDialogElement {
  $: {
    close: CrIconButtonElement,
    dialog: HTMLDialogElement,
  };
}

export class CrDialogElement extends CrDialogElementBase {
  static get is() {
    return 'cr-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      open: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Alt-text for the dialog close button.
       */
      closeText: String,

      /**
       * True if the dialog should remain open on 'popstate' events. This is
       * used for navigable dialogs that have their separate navigation handling
       * code.
       */
      ignorePopstate: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the dialog should ignore 'Enter' keypresses.
       */
      ignoreEnterKey: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the dialog should consume 'keydown' events. If ignoreEnterKey
       * is true, 'Enter' key won't be consumed.
       */
      consumeKeydownEvent: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the dialog should not be able to be cancelled, which will
       * prevent 'Escape' key presses from closing the dialog.
       */
      noCancel: {
        type: Boolean,
        value: false,
      },

      // True if dialog should show the 'X' close button.
      showCloseButton: {
        type: Boolean,
        value: false,
      },

      showOnAttach: {
        type: Boolean,
        value: false,
      },

      /**
       * Text for the aria description.
       */
      ariaDescriptionText: String,
    };
  }

  closeText: string;
  consumeKeydownEvent: boolean;
  ignoreEnterKey: boolean;
  ignorePopstate: boolean;
  noCancel: boolean;
  open: boolean;
  showCloseButton: boolean;
  showOnAttach: boolean;
  ariaDescriptionText: string;

  private intersectionObserver_: IntersectionObserver|null = null;
  private mutationObserver_: MutationObserver|null = null;
  private boundKeydown_: ((e: KeyboardEvent) => void)|null = null;

  override ready() {
    super.ready();

    // If the active history entry changes (i.e. user clicks back button),
    // all open dialogs should be cancelled.
    window.addEventListener('popstate', () => {
      if (!this.ignorePopstate && this.$.dialog.open) {
        this.cancel();
      }
    });

    if (!this.ignoreEnterKey) {
      this.addEventListener('keypress', this.onKeypress_.bind(this));
    }
    this.addEventListener('pointerdown', e => this.onPointerdown_(e));
  }

  override connectedCallback() {
    super.connectedCallback();
    const mutationObserverCallback = () => {
      if (this.$.dialog.open) {
        this.enableShadowBehavior(true);
        this.addKeydownListener_();
      } else {
        this.enableShadowBehavior(false);
        this.removeKeydownListener_();
      }
    };

    this.mutationObserver_ = new MutationObserver(mutationObserverCallback);

    this.mutationObserver_.observe(this.$.dialog, {
      attributes: true,
      attributeFilter: ['open'],
    });

    // In some cases dialog already has the 'open' attribute by this point.
    mutationObserverCallback();
    if (this.showOnAttach) {
      this.showModal();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeKeydownListener_();
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }
  }

  private addKeydownListener_() {
    if (!this.consumeKeydownEvent) {
      return;
    }

    this.boundKeydown_ = this.boundKeydown_ || this.onKeydown_.bind(this);

    this.addEventListener('keydown', this.boundKeydown_);

    // Sometimes <body> is key event's target and in that case the event
    // will bypass cr-dialog. We should consume those events too in order to
    // behave modally. This prevents accidentally triggering keyboard commands.
    document.body.addEventListener('keydown', this.boundKeydown_);
  }

  private removeKeydownListener_() {
    if (!this.boundKeydown_) {
      return;
    }

    this.removeEventListener('keydown', this.boundKeydown_);
    document.body.removeEventListener('keydown', this.boundKeydown_);
    this.boundKeydown_ = null;
  }

  showModal() {
    this.$.dialog.showModal();
    assert(this.$.dialog.open);
    this.open = true;
    this.dispatchEvent(
        new CustomEvent('cr-dialog-open', {bubbles: true, composed: true}));
  }

  cancel() {
    this.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
    this.$.dialog.close();
    assert(!this.$.dialog.open);
    this.open = false;
  }

  close() {
    this.$.dialog.close('success');
    assert(!this.$.dialog.open);
    this.open = false;
  }

  /**
   * Set the title of the dialog for a11y reader.
   * @param title Title of the dialog.
   */
  setTitleAriaLabel(title: string) {
    this.$.dialog.removeAttribute('aria-labelledby');
    this.$.dialog.setAttribute('aria-label', title);
  }

  private onCloseKeypress_(e: Event) {
    // Because the dialog may have a default Enter key handler, prevent
    // keypress events from bubbling up from this element.
    e.stopPropagation();
  }

  private onNativeDialogClose_(e: Event) {
    // Ignore any 'close' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative()) {
      return;
    }

    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.dispatchEvent(
        new CustomEvent('close', {bubbles: true, composed: true}));
  }

  private onNativeDialogCancel_(e: Event) {
    // Ignore any 'cancel' events not fired directly by the <dialog> element.
    if (e.target !== this.getNative()) {
      return;
    }

    if (this.noCancel) {
      e.preventDefault();
      return;
    }

    // When the dialog is dismissed using the 'Esc' key, need to manually update
    // the |open| property (since close() is not called).
    this.open = false;

    // Catch and re-fire the native 'cancel' event such that it bubbles across
    // Shadow DOM v1.
    this.dispatchEvent(
        new CustomEvent('cancel', {bubbles: true, composed: true}));
  }

  /**
   * Expose the inner native <dialog> for some rare cases where it needs to be
   * directly accessed (for example to programmatically setheight/width, which
   * would not work on the wrapper).
   */
  getNative(): HTMLDialogElement {
    return this.$.dialog;
  }

  private onKeypress_(e: KeyboardEvent) {
    if (e.key !== 'Enter') {
      return;
    }

    // Accept Enter keys from either the dialog itself, or a child cr-input,
    // considering that the event may have been retargeted, for example if the
    // cr-input is nested inside another element. Also exclude inputs of type
    // 'search', since hitting 'Enter' on a search field most likely intends to
    // trigger searching.
    const accept = e.target === this ||
        e.composedPath().some(
            el => (el as HTMLElement).tagName === 'CR-INPUT' &&
                (el as CrInputElement).type !== 'search');
    if (!accept) {
      return;
    }
    const actionButton = this.querySelector<HTMLElement>(
        '.action-button:not([disabled]):not([hidden])');
    if (actionButton) {
      actionButton.click();
      e.preventDefault();
    }
  }

  private onKeydown_(e: KeyboardEvent) {
    assert(this.consumeKeydownEvent);

    if (!this.getNative().open) {
      return;
    }

    if (this.ignoreEnterKey && e.key === 'Enter') {
      return;
    }

    // Stop propagation to behave modally.
    e.stopPropagation();
  }

  private onPointerdown_(e: PointerEvent) {
    // Only show pulse animation if user left-clicked outside of the dialog
    // contents.
    if (e.button !== 0 ||
        (e.composedPath()[0]! as HTMLElement).tagName !== 'DIALOG') {
      return;
    }

    this.$.dialog.animate(
        [
          {transform: 'scale(1)', offset: 0},
          {transform: 'scale(1.02)', offset: 0.4},
          {transform: 'scale(1.02)', offset: 0.6},
          {transform: 'scale(1)', offset: 1},
        ],
        {
          duration: 180,
          easing: 'ease-in-out',
          iterations: 1,
        });

    // Prevent any text from being selected within the dialog when clicking in
    // the backdrop area.
    e.preventDefault();
  }

  override focus() {
    const titleContainer =
        this.shadowRoot!.querySelector<HTMLElement>('.title-container');
    assert(titleContainer);
    titleContainer.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-dialog': CrDialogElement;
  }
}

customElements.define(CrDialogElement.is, CrDialogElement);
