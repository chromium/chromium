// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import type {ContextualUpload} from '//resources/cr_components/composebox/common.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {ComposeboxContextAddedMethod} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {PlaceholderTextCycler} from '//resources/cr_components/searchbox/placeholder_text_cycler.js';
import {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {waitForLazyRender} from '//resources/cr_components/searchbox/utils.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './ntp_searchbox.css.js';
import {getHtml} from './ntp_searchbox.html.js';

// The NTP Realbox entry point is always part of the Next experience, so log
// the source value with the "crn" component.
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE = 'chrome.crn.rb';
const DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE = '42';

interface ClickEventDetail {
  button: number;
  ctrlKey: boolean;
  metaKey: boolean;
  shiftKey: boolean;
}

/** A search box for the NTP that behaves like the Omnibox. */
export class NtpSearchboxElement extends SearchboxElement implements
    DragAndDropHost, SearchboxMixinInterface {
  static override get is() {
    return 'ntp-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties(): any {
    return {
      //========================================================================
      // Public properties
      //========================================================================
      ntpRealboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },

      composeboxEnabled: {type: Boolean},

      composeButtonEnabled: {type: Boolean},

      cyclingPlaceholders: {type: Boolean},

      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
    };
  }

  accessor ntpRealboxNextEnabled: boolean = false;
  accessor composeboxEnabled: boolean = false;
  accessor composeButtonEnabled: boolean = false;
  accessor cyclingPlaceholders: boolean = false;
  private accessor placeholderCycler_: PlaceholderTextCycler | null = null;
  accessor isDraggingFile: boolean = false;
  protected dragAndDropHandler: DragAndDropHandler|null = null;
  private dragAndDropEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');

  override async connectedCallback() {
    super.connectedCallback();

    if (this.ntpRealboxNextEnabled) {
      this.dragAndDropHandler =
          new DragAndDropHandler(this, this.dragAndDropEnabled_);
    }
    await Promise.resolve();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.placeholderCycler_?.stop();
  }

  override firstUpdated() {
    super.firstUpdated();

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler().getPlaceholderConfig();
        const texts = config.texts;
        if (texts.length === 0) {
          // PEC API returned no placeholders; feature is disabled.
          return;
        }
        this.placeholderText = texts[0]!;
        this.placeholderCycler_ = new PlaceholderTextCycler(
            this.$.input.inputElement, texts,
            Number(config.changeTextAnimationInterval.microseconds / 1000n),
            Number(config.fadeTextAnimationDuration.microseconds / 1000n));
        this.placeholderCycler_.start();
      });
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('composeButtonEnabled') ||
        changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ = this.composeButtonEnabled ||
          (this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline);
    }
  }

  protected override shouldShowVoiceLens_(isEnabled: boolean): boolean {
    return !(this.dropdownIsVisible && this.composeButtonEnabled) &&
        super.shouldShowVoiceLens_(isEnabled);
  }

  override handleKeyNavigation(e: KeyboardEvent) {
    if (this.composeButtonEnabled && e.key === 'Tab' &&
        this.$.input?.lastInput()?.inline &&
        this.$.input === this.shadowRoot.activeElement) {
      if (e.shiftKey) {
        this.$.input.setInput({inline: ''});
        return;
      }

      const newText =
          this.$.input.lastInput()!.text + this.$.input.lastInput()!.inline;
      this.$.input.setInput({
        text: newText,
        inline: '',
        moveCursorToEnd: true,
      });
      this.queryAutocomplete(newText, false);
      e.preventDefault();
      return;
    }

    super.handleKeyNavigation(e);
  }

  protected override onInputFocusin_() {
    super.onInputFocusin_();
    this.placeholderCycler_?.stop();
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    super.onInputWrapperFocusout(e);
    this.placeholderCycler_?.start();
  }

  getDropTarget() {
    return this;
  }

  addDroppedFiles(files: FileList) {
    this.processFiles_(files, ComposeboxContextAddedMethod.DRAG_AND_DROP);
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onComposeClick_(e: CustomEvent<ClickEventDetail>) {
    // TODO(crbug.com/463667769): Call submitQuery here since RealboxHandler is
    // now a `ContextualSearchboxHandler`.
    this.pageHandler().activateMetricsFunnel('AiModeButton');

    chrome.histograms.recordUserAction(
        'ContextualSearch.AiModeButtonClick.NtpRealbox');
    chrome.histograms.recordBoolean(
        'ContextualSearch.AiModeButtonClick.NtpRealbox', true);

    if (!this.composeboxEnabled || this.$.input.inputElement.value.trim()) {
      const histogramName =
          'ContextualSearch.UserAction.SubmitQueryV2.NewTabPage';
      // LINT.IfChange(ContextualSearchContextState)
      chrome.histograms.recordEnumerationValue(
          histogramName, /*WithoutContext */ 0, 3);
      // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchContextState)

      const userActionName =
          'ContextualSearch.UserAction.SubmitQueryV2.WithoutContext.NewTabPage';
      chrome.histograms.recordUserAction(userActionName);

      // Construct navigation url.
      const searchParams = new URLSearchParams();
      searchParams.append('sourceid', 'chrome');
      searchParams.append('udm', '50');
      searchParams.append('aep', DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT_VALUE);
      searchParams.append(
          'source', DESKTOP_CHROME_NTP_REALBOX_ENTRY_SOURCE_VALUE);

      if (this.$.input.inputElement.value.trim()) {
        searchParams.append('q', this.$.input.inputElement.value.trim());
      }
      const queryUrl =
          new URL('/search', loadTimeData.getString('googleBaseUrl'));
      queryUrl.search = searchParams.toString();
      const href = queryUrl.href;

      // Handle mouse events.
      if (e.detail.ctrlKey || e.detail.metaKey) {
        window.open(href, '_blank');
      } else if (e.detail.shiftKey) {
        window.open(href, '_blank', 'noopener');
      } else {
        window.open(href, '_self');
      }
    } else {
      this.openComposebox_();
    }

    chrome.histograms.recordBoolean(
        'NewTabPage.ComposeEntrypoint.Click.UserTextPresent',
        !this.isInputEmpty());
  }

  protected override openComposebox_(
      uploads: ContextualUpload[] = [], mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified) {
    if (this.ntpRealboxNextEnabled) {
      const context =
          this.shadowRoot.querySelector<ContextualEntrypointAndMenuElement>(
              '#context');
      assert(context);
      context.closeMenu();
    }
    super.openComposebox_(uploads, mode, model);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-searchbox': NtpSearchboxElement;
  }
}

customElements.define(NtpSearchboxElement.is, NtpSearchboxElement);
