// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './pinned_toolbar_action.js';
import './toolbar_divider.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {PinnedToolbarActionElement} from './pinned_toolbar_action.js';
import {getCss} from './pinned_toolbar_actions.css.js';
import {getHtml} from './pinned_toolbar_actions.html.js';

// State pushed to Lit template for rendering.
export interface KeyedActionState {
  // Key so repeat directive can maintain consistent mapping between this
  // particular state and the Lit element.
  key: string;
  // Most of the state of the Lit element.
  state: PinnedToolbarActionState;
  // Is this element sliding out (i.e. exiting)?
  // If true, this instance will be deleted from `keyedStates_` when the
  // slide-out animation completes by `onTransitionDone_()`.
  exiting?: boolean;
  // Should this element animate in (i.e. slide in)?
  animateIn?: boolean;
  // Is this element currently being dragged (rendering as gap/placeholder)?
  dragPlaceholder?: boolean;
}

export class PinnedToolbarActionsElement extends CrLitElement {
  static get is() {
    return 'pinned-toolbar-actions';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state: {type: Array},
      keyedStates_: {type: Array},
    };
  }

  accessor state: PinnedToolbarActionState[] = [];

  // Internal reactive state that includes exiting items.
  protected accessor keyedStates_: KeyedActionState[] = [];

  private draggedActionId_: PinnedToolbarAction|null = null;
  private dragEnterCount_ = 0;
  private didDrop_ = false;
  private actionToFocusAfterUpdate_: PinnedToolbarAction|null = null;


  // Channel to coordinate drag-and-drop state across different browser windows.
  private dragChannel_: BroadcastChannel|null = null;

  // The action ID currently being dragged in *another* window, received via
  // the BroadcastChannel. Null if no external drag is active.
  private externallyDraggedActionId_: PinnedToolbarAction|null = null;

  private mouseMoveListener_ = () => this.onWindowMouseMove_();

  override connectedCallback() {
    super.connectedCallback();
    // Initialize the BroadcastChannel for cross-window drag sync.
    this.dragChannel_ = new BroadcastChannel('pinned-action-drag');
    this.dragChannel_.onmessage = (e) => this.onDragChannelMessage_(e);
    window.addEventListener('mousemove', this.mouseMoveListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.dragChannel_) {
      this.dragChannel_.close();
      this.dragChannel_ = null;
    }
    window.removeEventListener('mousemove', this.mouseMoveListener_);
  }

  private setPlaceholder_(actionId: PinnedToolbarAction) {
    this.keyedStates_ = this.keyedStates_.map(s => {
      if (s.state.action === actionId) {
        return {...s, dragPlaceholder: true};
      }
      return s;
    });
  }

  private clearPlaceholderFlagsOnly_() {
    this.keyedStates_ = this.keyedStates_.map(s => {
      if (s.dragPlaceholder) {
        const {dragPlaceholder: _dragPlaceholder, ...rest} = s;
        return rest;
      }
      return s;
    });
  }

  private clearPlaceholderAndRevert_() {
    this.clearPlaceholderFlagsOnly_();
    this.reconcileKeys_();
  }

  // Handles drag state updates broadcasted from other toolbar instances,
  // allowing us to sync and track cross-window drags.
  private onDragChannelMessage_(e: MessageEvent) {
    if (e.data.type === 'drag-start') {
      this.externallyDraggedActionId_ = e.data.actionId;
      // Guard against race: if the drag already entered this window before we
      // received the broadcast, apply the placeholder now.
      if (this.dragEnterCount_ > 0) {
        this.setPlaceholder_(e.data.actionId);
      }
    } else if (e.data.type === 'drag-end') {
      this.externallyDraggedActionId_ = null;
      if (e.data.aborted) {
        this.clearPlaceholderAndRevert_();
      } else {
        this.clearPlaceholderFlagsOnly_();
      }
    }
  }

  // A fallback listener to guard against the browser or OS failing to fire
  // the native 'dragend' event (e.g., if the drag is aborted over a
  // non-chrome window). During an active browser drag session, standard
  // 'mousemove' events are suppressed. If we receive a 'mousemove' while
  // we still think a drag is active, we know the drag must have ended
  // externally, so we force state cleanup.
  private onWindowMouseMove_() {
    if (this.draggedActionId_ !== null ||
        this.externallyDraggedActionId_ !== null) {
      this.draggedActionId_ = null;
      this.externallyDraggedActionId_ = null;
      this.clearPlaceholderAndRevert_();
      if (this.dragChannel_) {
        this.dragChannel_.postMessage({
          type: 'drag-end',
          aborted: true,
        });
      }
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('state')) {
      const isDragging = this.draggedActionId_ !== null ||
          this.externallyDraggedActionId_ !== null;
      if (isDragging) {
        // State updates invalidate the layout index order, making subsequent
        // Mojo reorder calls unsafe. We must abort the drag session.
        const draggedActionId =
            this.draggedActionId_ ?? this.externallyDraggedActionId_;
        if (draggedActionId !== null) {
          // HTML5 DND has no programmatic cancel method. Force-removing the
          // drag source element from the DOM tells the browser to abort the
          // native OS drag session. Lit will automatically recreate the element
          // in its new position during the subsequent reconcileKeys_ update.
          const el = this.shadowRoot.querySelector(
              `pinned-toolbar-action[data-key="${draggedActionId}"]`);
          if (el) {
            el.remove();
          }
        }
        this.draggedActionId_ = null;
        this.externallyDraggedActionId_ = null;
        this.clearPlaceholderFlagsOnly_();
        if (this.dragChannel_) {
          this.dragChannel_.postMessage({
            type: 'drag-end',
            aborted: true,
          });
        }
      }
      this.reconcileKeys_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    // Add listener to shadow root to catch bubbled transitionend and
    // transitioncancel events.
    this.shadowRoot.addEventListener(
        'transitionend', (e) => this.onTransitionDone_(e as TransitionEvent));
    this.shadowRoot.addEventListener(
        'transitioncancel',
        (e) => this.onTransitionDone_(e as TransitionEvent));

    // When a child action initiates dragging, we mark it locally as a
    // placeholder (making it invisible to act as a visual gap) and broadcast
    // the start event to sync other window toolbar instances.
    this.addEventListener('pinned-action-drag-start', (e: Event) => {
      const customEvent = e as CustomEvent<{action: PinnedToolbarAction}>;
      this.draggedActionId_ = customEvent.detail.action;
      if (this.dragChannel_) {
        this.dragChannel_.postMessage({
          type: 'drag-start',
          actionId: this.draggedActionId_,
        });
      }
      this.setPlaceholder_(this.draggedActionId_);
    });

    // When the drag session ends, we notify other windows to clear their state,
    // clear our local dragged ID, and restore the placeholder element to
    // normal.
    this.addEventListener('pinned-action-drag-end', (e: Event) => {
      if (!this.draggedActionId_) {
        return;
      }
      const customEvent = e as CustomEvent<{
                            action: PinnedToolbarAction,
                            dropEffect: string | undefined,
                          }>;
      const dropEffect = customEvent.detail?.dropEffect;

      const aborted = dropEffect !== 'move';

      if (this.dragChannel_) {
        this.dragChannel_.postMessage({
          type: 'drag-end',
          aborted: aborted,
        });
      }
      this.draggedActionId_ = null;
      this.didDrop_ = false;

      if (aborted) {
        this.clearPlaceholderAndRevert_();
      } else {
        this.clearPlaceholderFlagsOnly_();
      }
    });


    this.addEventListener('pinned-action-keyboard-reorder', (e: Event) => {
      const customEvent = e as CustomEvent<{action: PinnedToolbarAction}>;
      this.actionToFocusAfterUpdate_ = customEvent.detail.action;
    });

    // We attach dragenter/dragleave/drop listeners to the host element
    // programmatically because we need to track when the drag enters or leaves
    // the boundaries of the entire container (using dragEnterCount_).
    // Child-level listeners cannot detect container-level exits. Individual
    // item dragover/drop handling is declared on the children in the HTML
    // template for declarative clarity and easier target binding.
    this.addEventListener('dragenter', (e) => this.onHostDragenter_(e));
    this.addEventListener('dragleave', (e) => this.onHostDragleave_(e));
    this.addEventListener('dragover', (e) => this.onHostDragover_(e));
    this.addEventListener('drop', (e) => this.onHostDrop_(e));
  }

  private reconcileKeys_() {
    const newMojoStates = this.state || [];
    const isInitial = this.keyedStates_.length === 0 &&
        // Initial updates contain only pinned items, which requires a divider.
        newMojoStates.some(s => s.action === PinnedToolbarAction.kDivider);

    // 1. Map new mojo states to KeyedActionState (all active).
    const newKeyedStates: KeyedActionState[] = newMojoStates.map(s => {
      const key = s.action.toString();
      const animateIn = !isInitial &&
          !this.keyedStates_.some(old => old.key === key && !old.animateIn);
      return {key, state: s, animateIn};
    });

    // 2. Find which keys were in the old `keyedStates_` but are not in
    // `newKeyedStates`. These are the ones that are "sliding-out".
    const newKeys = new Set(newKeyedStates.map(s => s.key));
    const missingOldStates = this.keyedStates_.filter(s => !newKeys.has(s.key));

    // 3. Re-insert "sliding-out" states (marked as exiting) into their old
    // positions in `keyedStates_` so they'll be rendered while "sliding-out"
    // if animations are enabled.
    const showAnimations = getComputedStyle(this)
                               .getPropertyValue('--animations-enabled')
                               .trim() !== '0';

    if (showAnimations) {
      // Sort missing states by their original index to preserve order during
      // insertion
      const oldKeyToIndex =
          new Map(this.keyedStates_.map((s, i) => [s.key, i]));
      missingOldStates.sort(
          (a, b) => oldKeyToIndex.get(a.key)! - oldKeyToIndex.get(b.key)!);

      // Insert them back with `exiting` set to true.
      for (const missing of missingOldStates) {
        const exitingState = {...missing, exiting: true};
        const originalIndex = oldKeyToIndex.get(missing.key)!;
        const insertIndex = Math.min(originalIndex, newKeyedStates.length);
        newKeyedStates.splice(insertIndex, 0, exitingState);
      }
    }

    this.keyedStates_ = newKeyedStates;
    this.updateVisibility_();

    // If the layout engine has already forced the exiting elements to 0 width
    // (preempting the transition), or if animations are disabled, remove them
    // immediately.
    this.updateComplete.then(() => {
      for (const el of this.shadowRoot.querySelectorAll('.exiting')) {
        const htmlEl = el as HTMLElement;
        // If it's already 0px wide, it won't transition.
        if (htmlEl.getBoundingClientRect().width === 0) {
          const key = htmlEl.dataset['key'];
          if (key) {
            this.keyedStates_ = this.keyedStates_.filter(s => s.key !== key);
          }
        }
      }
      this.updateVisibility_();
    });
  }

  // When an element finishes "sliding-out", remove it from `keyedStates_`.
  private onTransitionDone_(e: TransitionEvent) {
    // We only care about the width transition to trigger removal
    if (e.propertyName !== 'width') {
      return;
    }

    const target = e.target as HTMLElement;
    if (!target.classList.contains('exiting')) {
      return;
    }

    const key = target.dataset['key'];
    if (!key) {
      return;
    }

    // Remove the finished item (automatically triggers update)
    this.keyedStates_ = this.keyedStates_.filter(s => s.key !== key);
    this.updateVisibility_();
  }

  private updateVisibility_() {
    this.hidden = this.keyedStates_.length === 0;
  }

  protected onActionDragover_(e: DragEvent) {
    if (!e.dataTransfer) {
      return;
    }
    if (!e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      return;
    }
    // Identify which action is being dragged, checking first if it is a local
    // drag (draggedActionId_) or a cross-window drag
    // (externallyDraggedActionId_).
    const draggedActionId =
        this.draggedActionId_ ?? this.externallyDraggedActionId_;
    if (draggedActionId === null) {
      return;
    }
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
    const target = e.currentTarget as HTMLElement;

    const key = target?.dataset['key'];
    const overState = this.keyedStates_.find(s => s.key === key);
    if (!overState || overState.state.action === PinnedToolbarAction.kDivider) {
      return;
    }
    // If the user hovers over the dragged action itself, do nothing. It is
    // already marked as a placeholder on dragstart or host dragenter.
    if (draggedActionId === overState.state.action) {
      return;
    }

    const fromIndex =
        this.keyedStates_.findIndex(s => s.state.action === draggedActionId);

    const toIndex = this.keyedStates_.findIndex(s => s.key === overState.key);

    // Re-order the actions to show what would happen on drop.
    if (fromIndex !== -1 && toIndex !== -1 && fromIndex !== toIndex) {
      const newStates = [...this.keyedStates_];
      const [moved] = newStates.splice(fromIndex, 1);
      if (moved) {
        newStates.splice(toIndex, 0, moved);
      }
      this.keyedStates_ = newStates;
    }
  }

  protected onActionDrop_(e: DragEvent) {
    if (!e.dataTransfer ||
        !e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      return;
    }

    try {
      const data = JSON.parse(
          e.dataTransfer.getData('application/x-webui-pinned-action'));
      const actionId = data.actionId;
      if (actionId === undefined || actionId === null) {
        return;
      }

      // Guard: Dropped action ID must match what we are tracking as currently
      // dragged locally or externally.
      const draggedActionId =
          this.draggedActionId_ ?? this.externallyDraggedActionId_;
      if (actionId !== draggedActionId) {
        return;
      }

      e.preventDefault();
      e.stopPropagation();

      this.dragEnterCount_ = 0;
      this.didDrop_ = true;

      const nonDividerStates = this.keyedStates_.filter(
          s => s.state.action !== PinnedToolbarAction.kDivider);
      const placeholderIdx = nonDividerStates.findIndex(s => s.dragPlaceholder);
      if (placeholderIdx !== -1) {
        const proxy = BrowserProxyImpl.getInstance();
        proxy.toolbarUIHandler.movePinnedToolbarAction(
            actionId, placeholderIdx);
      }
    } catch (_err) {
      this.draggedActionId_ = null;
    }
  }

  protected onHostDragenter_(e: DragEvent) {
    if (!e.dataTransfer ||
        !e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      return;
    }
    this.dragEnterCount_++;
    if (this.dragEnterCount_ === 1) {
      const draggedActionId =
          this.draggedActionId_ ?? this.externallyDraggedActionId_;
      if (draggedActionId !== null) {
        this.setPlaceholder_(draggedActionId);
      }
    }
  }

  protected onHostDragleave_(e: DragEvent) {
    if (!e.dataTransfer ||
        !e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      return;
    }
    this.dragEnterCount_--;
    if (this.dragEnterCount_ === 0) {
      this.reconcileKeys_();
    }
  }

  protected onHostDragover_(e: DragEvent) {
    if (e.dataTransfer &&
        e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      e.preventDefault();
      e.dataTransfer.dropEffect = 'move';
    }
  }

  protected onHostDrop_(e: DragEvent) {
    this.dragEnterCount_ = 0;
    if (this.didDrop_) {
      return;
    }

    if (!e.dataTransfer ||
        !e.dataTransfer.types.includes('application/x-webui-pinned-action')) {
      this.reconcileKeys_();
      return;
    }

    try {
      const data = JSON.parse(
          e.dataTransfer.getData('application/x-webui-pinned-action'));
      const actionId = data.actionId;
      if (actionId === undefined || actionId === null) {
        this.reconcileKeys_();
        return;
      }

      const draggedActionId =
          this.draggedActionId_ ?? this.externallyDraggedActionId_;
      if (actionId !== draggedActionId) {
        this.reconcileKeys_();
        return;
      }

      if (draggedActionId !== null) {
        const nonDividerStates = this.keyedStates_.filter(
            s => s.state.action !== PinnedToolbarAction.kDivider);
        const placeholderIdx =
            nonDividerStates.findIndex(s => s.dragPlaceholder);
        if (placeholderIdx !== -1) {
          e.preventDefault();
          this.didDrop_ = true;
          const proxy = BrowserProxyImpl.getInstance();
          proxy.toolbarUIHandler.movePinnedToolbarAction(
              draggedActionId, placeholderIdx);
          return;
        }
      }
    } catch (_err) {
      // Ignore parse errors
    }

    this.reconcileKeys_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    // If a keyboard reorder event was captured, we need to restore focus to the
    // moved action button. We must wait for the DOM update to complete
    // (updateComplete promise) so that the elements are in their new positions.
    if (this.actionToFocusAfterUpdate_ !== null) {
      const actionId = this.actionToFocusAfterUpdate_;
      this.actionToFocusAfterUpdate_ = null;

      this.updateComplete.then(() => {
        const el = this.shadowRoot.querySelector(
            `pinned-toolbar-action[data-key="${actionId}"]`);
        if (el) {
          (el as PinnedToolbarActionElement).focus();
        }
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-actions': PinnedToolbarActionsElement;
  }
}

customElements.define(
    PinnedToolbarActionsElement.is, PinnedToolbarActionsElement);
