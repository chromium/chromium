// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-styles/color.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '../common_styles/oobe_common_styles.css.js';
import '../common_styles/oobe_dialog_host_styles.css.js';
import '../oobe_vars/oobe_shared_vars.css.js';

import {CrLazyRenderElement} from '//resources/ash/common/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeFocusBehavior, OobeFocusBehaviorInterface} from '../behaviors/oobe_focus_behavior.js';
import {OobeScrollableBehavior, OobeScrollableBehaviorInterface} from '../behaviors/oobe_scrollable_behavior.js';

import {getTemplate} from './oobe_content_dialog.html.js';

const OobeContentDialogBase =
    mixinBehaviors(
        [OobeFocusBehavior, OobeScrollableBehavior], PolymerElement) as {
      new (): PolymerElement & OobeFocusBehaviorInterface &
          OobeScrollableBehaviorInterface,
    };

export class OobeContentDialog extends OobeContentDialogBase {
  static get is() {
    return 'oobe-content-dialog' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Supports dialog which is shown without buttons.
       */
      noButtons: {
        type: Boolean,
        value: false,
      },

      /**
       * If set, prevents lazy instantiation of the dialog.
       */
      noLazy: {
        type: Boolean,
        value: false,
        observer: 'onNoLazyChanged',
      },
    };
  }

  private noButtons: boolean;
  private noLazy: boolean;

  private getLazyRender(): CrLazyRenderElement<HTMLElement> {
    const lazyRender = this.shadowRoot?.querySelector('#lazy');
    assert(lazyRender instanceof CrLazyRenderElement);
    return lazyRender;
  }

  onBeforeShow(): void {
    this.getLazyRender().get();
    const contentContainer =
        this.shadowRoot?.querySelector('#contentContainer');
    const scrollContainer = this.shadowRoot?.querySelector('#scrollContainer');
    if (!scrollContainer || !contentContainer) {
      return;
    }
    this.initScrollableObservers(scrollContainer, contentContainer);
  }

  override focus(): void {
    /**
     * TODO (crbug.com/1159721): Fix this once event flow of showing step in
     * display_manager is updated.
     */
    this.show();
  }

  show(): void {
    this.focusMarkedElement(this);
  }

  private onNoLazyChanged(): void {
    if (this.noLazy) {
      this.getLazyRender().get();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeContentDialog.is]: OobeContentDialog;
  }
}

customElements.define(OobeContentDialog.is, OobeContentDialog);
