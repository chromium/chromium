// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_spinner_style.css.js';
import '/strings.m.js';
import './searchbox_shared_style.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PageContentType} from './page_content_type.mojom-webui.js';
import {getTemplate} from './searchbox_ghost_loader.html.js';
import {BrowserProxyImpl} from './searchbox_ghost_loader_browser_proxy.js';
import type {BrowserProxy} from './searchbox_ghost_loader_browser_proxy.js';

const SearchboxGhostLoaderElementBase = I18nMixin(PolymerElement);

// Displays a loading preview while waiting on autocomplete to return matches.
export class SearchboxGhostLoaderElement extends
    SearchboxGhostLoaderElementBase {
  static get is() {
    // LINT.IfChange(GhostLoaderTagName)
    return 'cr-searchbox-ghost-loader';
    // LINT.ThenChange(/ui/webui/resources/cr_components/searchbox/searchbox.ts:GhostLoaderTagName)
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showErrorState: {
        type: Boolean,
        reflectToAttribute: true,
        notify: true,
      },
      showContextualSearchboxLoadingState: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('showContextualSearchboxLoadingState'),
        reflectToAttribute: true,
      },
      pageContentType: {
        type: String,
        value: PageContentType.kUnknown,
      },
      ghostLoaderPrimaryMessage: {
        type: String,
        computed: `computeGhostLoaderPrimaryMessage(pageContentType)`,
      },
    };
  }

  // Whether the autocomplete stop timer has triggered. If it has, we should
  // hide the ghost loader. We also show the error text in this case.
  private showErrorState: boolean;
  private showContextualSearchboxLoadingState: boolean;
  // What the current page content type is.
  private pageContentType: PageContentType;
  private browserProxy: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds: number[];

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

    return this.computeGhostLoaderPrimaryMessage();
  }
  // LINT.ThenChange(//chrome/browser/resources/lens/shared/searchbox_ghost_loader.html:GhostLoaderText)

  showErrorStateForTesting() {
    this.showErrorState = true;
  }

  private computeGhostLoaderPrimaryMessage(): string {
    return this.pageContentType === PageContentType.kPdf ?
        this.i18n('searchboxGhostLoaderHintTextPrimaryPdf') :
        this.i18n('searchboxGhostLoaderHintTextPrimaryDefault');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-searchbox-ghost-loader': SearchboxGhostLoaderElement;
  }
}

customElements.define(
    SearchboxGhostLoaderElement.is, SearchboxGhostLoaderElement);
