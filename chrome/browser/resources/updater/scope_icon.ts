// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {localizeScope} from './event_history.js';
import type {Scope} from './event_history.js';
import {getHtml} from './scope_icon.html.js';

export class ScopeIconElement extends CrLitElement {
  static get is() {
    return 'scope-icon';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      scope: {type: String},
    };
  }

  accessor scope: Scope|undefined = undefined;

  protected icon: string|undefined = undefined;
  protected label: string|undefined = undefined;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('scope')) {
      this.icon = this.computeIcon();
      this.label = this.computeLabel();
    }
  }

  computeIcon(): string {
    assert(this.scope !== undefined);
    return this.scope === 'SYSTEM' ? 'cr:computer' : 'cr:person';
  }

  computeLabel(): string {
    assert(this.scope !== undefined);
    return localizeScope(this.scope);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'scope-icon': ScopeIconElement;
  }
}

customElements.define(ScopeIconElement.is, ScopeIconElement);
