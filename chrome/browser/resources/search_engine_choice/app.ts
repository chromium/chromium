// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://search-engine-choice/tangible_sync_style_shared.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_radio_group/cr_radio_group.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {SearchEngineChoice, SearchEngineChoiceBrowserProxy} from './browser_proxy.js';

export interface SearchEngineChoiceAppElement {
  $: {
    dummyOmnibox: HTMLElement,
    infoDialog: CrDialogElement,
    searchEngineOmnibox: HTMLElement,
    submitButton: CrButtonElement,
  };
}

const SearchEngineChoiceAppElementBase = I18nMixin(PolymerElement);

export class SearchEngineChoiceAppElement extends
    SearchEngineChoiceAppElementBase {
  static get is() {
    return 'search-engine-choice-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * We pass the choice list as JSON because it doesn't change
       * dynamically, so it would be better to have it available as loadtime
       * data.
       */
      choiceList_: {
        type: Array,
        value() {
          return JSON.parse(loadTimeData.getString('choiceList'));
        },
      },

      // The choice will always be > 0 when selected for prepopulated engines
      // and == 0 for a custom search engine.
      selectedChoice_: {
        type: Number,
        value: -1,
        observer: 'onSelectedChoiceChanged_',
      },

      isSubmitDisabled_: {
        type: Boolean,
        computed: 'isSubmitButtonDisabled_(selectedChoice_)',
      },

      fakeOmniboxText_: {
        type: String,
        value: '',
      },

      fakeOmniboxIconPath_: {
        type: String,
        value: '',
      },
    };
  }

  private choiceList_: SearchEngineChoice[];
  private selectedChoice_: string;
  private fakeOmniboxText_: string;
  private fakeOmniboxIconPath_: string;

  override connectedCallback() {
    super.connectedCallback();

    // Change the `icon_path` format so that we can use it with the
    // `background-image` property in HTML. We need to use the
    // `background-image` property because `getFaviconForPageURL` returns an
    // `image-set` and not a url.
    this.choiceList_.forEach((searchEngine: SearchEngineChoice) => {
      if (searchEngine.prepopulate_id === 0) {
        // We get the favicon from the Favicon Service for custom search
        // engines.
        searchEngine.icon_path =
            getFaviconForPageURL(searchEngine?.url!, false, '', 24);
      } else {
        searchEngine.icon_path = 'url(' + searchEngine.icon_path + ')';
      }
    });

    afterNextRender(this, () => {
      SearchEngineChoiceBrowserProxy.getInstance().handler.displayDialog();
    });
  }

  private onLinkClicked_() {
    this.$.infoDialog.showModal();
  }

  private isSubmitButtonDisabled_() {
    return parseInt(this.selectedChoice_) === -1;
  }

  private onSubmitClicked_() {
    SearchEngineChoiceBrowserProxy.getInstance()
        .handler.handleSearchEngineChoiceSelected(
            parseInt(this.selectedChoice_));
  }

  private onInfoDialogButtonClicked_() {
    this.$.infoDialog.close();
  }

  private onSelectedChoiceChanged_(selectedChoice: string) {
    // No search engine selected.
    if (parseInt(selectedChoice) === -1) {
      return;
    }

    // Get the selected engine.
    const choice = this.choiceList_.find(
        elem => elem.prepopulate_id === parseInt(selectedChoice));
    const searchEngineOmnibox = this.$.searchEngineOmnibox;
    const dummyOmnibox = this.$.dummyOmnibox;
    const fakeOmniboxText = this.i18n('fakeOmniboxText', choice?.name!);
    const fakeOmniboxIconPath = choice?.icon_path!;

    // We need to change the previous engine name to the new one and then start
    // the fade-in-animation when the fade-out-animation finishes running.
    const handleFadeOutFinished = (event: AnimationEvent) => {
      if (event.animationName === 'fade-out-animation') {
        searchEngineOmnibox.classList.remove('fade-out-animation');

        this.fakeOmniboxText_ = fakeOmniboxText;
        this.fakeOmniboxIconPath_ = fakeOmniboxIconPath;
        // We call `requestAnimationFrame` to make sure that the previous
        // animation is fully removed so that we can run the next one.
        window.requestAnimationFrame(function() {
          searchEngineOmnibox.classList.add('fade-in-animation');
        });
      } else if (event.animationName === 'fade-in-animation') {
        // Hide the dummy omnibox so that we don't see it behind the search
        // engine omnibox.
        if (!dummyOmnibox.classList.contains('hidden')) {
          dummyOmnibox.classList.add('hidden');
        }
      }
    };

    // Show the dummy omnibox at fade-out start so that we can see it while
    // animating the search engine omnibox.
    const handleAnimationStart = (event: AnimationEvent) => {
      if (event.animationName === 'fade-out-animation') {
        dummyOmnibox.classList.remove('hidden');
      }
    };
    searchEngineOmnibox.addEventListener('animationend', handleFadeOutFinished);
    searchEngineOmnibox.addEventListener(
        'animationstart', handleAnimationStart);

    if (searchEngineOmnibox.classList.contains('fade-in-animation')) {
      searchEngineOmnibox.classList.remove('fade-in-animation');

      window.requestAnimationFrame(function() {
        searchEngineOmnibox.classList.add('fade-out-animation');
      });
    } else {
      this.fakeOmniboxText_ = fakeOmniboxText;
      this.fakeOmniboxIconPath_ = fakeOmniboxIconPath;
      searchEngineOmnibox.classList.add('fade-in-animation');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-engine-choice-app': SearchEngineChoiceAppElement;
  }
}

customElements.define(
    SearchEngineChoiceAppElement.is, SearchEngineChoiceAppElement);
