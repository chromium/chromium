// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {type Range as MojomRange} from '//resources/mojo/ui/gfx/range/mojom/range.mojom-webui.js';

import type {OmniboxViewState} from './browser_proxy.js';
import {getCss} from './readonly_omnibox.css.js';
import {getHtml} from './readonly_omnibox.html.js';

enum UserSelectionStatus {
  NOT_SET = 0,
  EXPLICITLY_CLEARED = 1
}

export interface ReadonlyOmniboxElement {
  $: {
    textContainer: HTMLElement,
  };
}

export class ReadonlyOmniboxElement extends CrLitElement {
  static get is() {
    return 'readonly-omnibox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxViewState: {type: Object},
    };
  }

  accessor omniboxViewState: OmniboxViewState = {
    text: '',
    selection: null,
  };

  private eventTracker: EventTracker = new EventTracker();

  // Selection state set here, rather than gotten from the browser.
  private userSelection: MojomRange|UserSelectionStatus =
      UserSelectionStatus.NOT_SET;

  override connectedCallback() {
    super.connectedCallback();

    this.eventTracker.add(
        document, 'selectionchange', this.onSelectionChange.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    this.addEventListener('blur', this.onBlur.bind(this));
    this.addEventListener('focus', this.onFocus.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('omniboxViewState')) {
      // We got fresh state from browser, so drop saved user selection ---
      // either the browser changed the URL, so it's not gonna make sense,
      // or it specifically wants specific things selected (or not).
      this.userSelection = UserSelectionStatus.NOT_SET;
      this.updateSelection();
    }
  }

  // Update displayed selection/caret based on browser-provided state
  // (`this.omniboxViewState`) and saved user selection (`this.userSelection`),
  // clearing it if the omnibox is not focused.
  // In the latter case, also makes sure to scroll long URLs to beginning.
  private updateSelection() {
    const textContainer = this.$.textContainer;
    let selectionState: MojomRange|null;
    if (this.userSelection === UserSelectionStatus.EXPLICITLY_CLEARED) {
      selectionState = null;
    } else if (this.userSelection === UserSelectionStatus.NOT_SET) {
      selectionState = this.omniboxViewState.selection;
    } else {
      selectionState = this.userSelection;
    }

    const selection = document.getSelection();
    if (!selection) {
      return;
    }

    // Range doesn't have Views's backwards selection feature, so normalize.
    if (selectionState && selectionState.start > selectionState.end) {
      selectionState = {start: selectionState.end, end: selectionState.start};
    }

    if (this.hasFocus() && selectionState) {
      selection.removeAllRanges();
      const text = this.firstTextChild();
      if (text) {
        const range = document.createRange();
        range.setStart(text, selectionState.start);
        range.setEnd(text, selectionState.end);
        selection.addRange(range);
      }
    } else if (selection.containsNode(
                   textContainer, /*allowPartialContainment=*/ true)) {
      // Make sure we make the beginning of the line visible.
      textContainer.scrollLeft = 0;

      // We should clear selection if it's with us and we're not focused or
      // it's blank in state.
      selection.removeAllRanges();
    }
  }

  private hasFocus(): boolean {
    return document.hasFocus() &&
        this.shadowRoot.activeElement === this.$.textContainer;
  }

  private onBlur(): void {
    this.updateSelection();
  }

  private onFocus(): void {
    // TODO(crbug.com/474060468): The Views behavior is pretty subtle here: it
    // mostly selects all but you can drag-select to get that specific
    // selection. Focus restore only seems to happen on keyboard focus.
    this.updateSelection();
  }

  // Called when the selection in the document is changed. If this happened when
  // this has the focus, update `state.userSelection` to match.
  //
  // TODO(crbug.com/474060468): We need to push selection to the browser. This
  // needs to worry about asynchrony.
  private onSelectionChange(): void {
    // TODO(crbug.com/474060468): This does weird stuff if other things can get
    // selection, which can hypothetically happen much down the line.

    // Ignore changes if we're not focused; this helps ignore our own
    // clearing of it in this case.
    if (!this.hasFocus()) {
      return;
    }

    const selection = document.getSelection();
    if (!selection) {
      return;
    }
    const ranges =
        selection.getComposedRanges({shadowRoots: [this.shadowRoot]});
    if (ranges.length === 0) {
      this.userSelection = UserSelectionStatus.EXPLICITLY_CLEARED;
      return;
    }

    assert(ranges.length === 1);
    assert(ranges[0]);
    const range = ranges[0];
    const text = this.firstTextChild();
    if (!text || !range.startContainer || !range.endContainer) {
      return;
    }

    this.userSelection = {
      start: range.startOffset,
      end: range.endOffset,
    };
  }

  // We need to dig a bit to find the actual text node since Lit has some
  // bookkeeping stuff around. When styling is added this will be generalized
  // to returning an array.
  private firstTextChild(): Node|null {
    const firstChildNode = this.$.textContainer.firstChild;
    if (!firstChildNode) {
      return null;
    }
    let node: Node|null = firstChildNode;
    while (node) {
      if (node.nodeType === Node.TEXT_NODE) {
        return node;
      }
      node = node.nextSibling;
    }
    return node;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'readonly-omnibox': ReadonlyOmniboxElement;
  }
}

customElements.define(ReadonlyOmniboxElement.is, ReadonlyOmniboxElement);
