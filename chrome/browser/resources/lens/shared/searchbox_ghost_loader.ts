// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '//resources/cr_components/searchbox/searchbox_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {PageContentType} from './page_content_type.mojom-webui.js';
import {getCss} from './searchbox_ghost_loader.css.js';
import {getHtml} from './searchbox_ghost_loader.html.js';
import {BrowserProxyImpl} from './searchbox_ghost_loader_browser_proxy.js';
import type {BrowserProxy} from './searchbox_ghost_loader_browser_proxy.js';

const SearchboxGhostLoaderElementBase = I18nMixinLit(CrLitElement);

// Displays a loading preview while waiting on autocomplete to return matches.
export class SearchboxGhostLoaderElement extends
    SearchboxGhostLoaderElementBase {
  static get is() {
    // LINT.IfChange(GhostLoaderTagName)
    return 'cr-searchbox-ghost-loader';
    // LINT.ThenChange(/ui/webui/resources/cr_components/searchbox/searchbox.ts:GhostLoaderTagName)
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableCsbMotionTweaks: {
        type: Boolean,
        reflect: true,
      },
      showErrorState: {
        type: Boolean,
        reflect: true,
        notify: true,
      },
      showContextualSearchboxLoadingState: {
        type: Boolean,
        reflect: true,
      },
      pageContentType: {type: Number},
      enableSummarizeSuggestionHint: {
        type: Boolean,
        reflect: true,
      },
      suggestionCount: {type: Number},
    };
  }

  // Whether the contextual searchbox motion tweaks are enabled via feature flag.
  private accessor enableCsbMotionTweaks: boolean =
      loadTimeData.getBoolean('enableCsbMotionTweaks');
  // Whether the autocomplete stop timer has triggered. If it has, we should
  // hide the ghost loader. We also show the error text in this case.
  private accessor showErrorState: boolean = false;
  protected accessor showContextualSearchboxLoadingState: boolean =
      loadTimeData.getBoolean('showContextualSearchboxLoadingState');
  // What the current page content type is.
  private accessor pageContentType: PageContentType = PageContentType.kUnknown;
  private accessor enableSummarizeSuggestionHint: boolean =
      loadTimeData.getBoolean('enableSummarizeSuggestionHint');
  // The number of suggestions to show in the ghost loader.
  private accessor suggestionCount: number = 0;

  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[] = [];

  override connectedCallback() {
    super.connectedCallback();

    const callbackRouter = this.browserProxy.callbackRouter;
    this.listenerIds = [
      callbackRouter.showErrorState.addListener(() => {
        this.showErrorState = true;
      }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds.forEach(
        id => assert(this.browserProxy.callbackRouter.removeListener(id)));
    this.listenerIds = [];
  }

  // LINT.IfChange(GhostLoaderText)
  getText(): string {
    if (!this.showContextualSearchboxLoadingState) {
      return this.i18n('searchboxGhostLoaderNoSuggestText');
    }

    if (this.showErrorState) {
      return this.i18n('searchboxGhostLoaderErrorText');
    }

    return this.getGhostLoaderPrimaryMessage();
  }
  // LINT.ThenChange(//chrome/browser/resources/lens/shared/searchbox_ghost_loader.html.ts:GhostLoaderText)

  showErrorStateForTesting() {
    this.showErrorState = true;
  }

  protected getGhostLoaderPrimaryMessage(): string {
    return this.pageContentType === PageContentType.kPdf ?
        this.i18n('searchboxGhostLoaderHintTextPrimaryPdf') :
        this.i18n('searchboxGhostLoaderHintTextPrimaryDefault');
  }

  protected getSuggestionItems(): number[] {
    if (this.suggestionCount === 0) {
      return Array(5).fill(0);
    }
    // The content of the array is unused.
    return Array(this.suggestionCount).fill(0);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-ghost-loader': SearchboxGhostLoaderElement;
  }
}

customElements.define(
    SearchboxGhostLoaderElement.is, SearchboxGhostLoaderElement);
