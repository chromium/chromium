// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import type {ContextualUpload, TabUpload, TabUploadOrigin} from '//resources/cr_components/composebox/common.js';
import {ContextType, GlifAnimationState, recordContextualElementClickedMetric, recordInputTypeShown, recordModelModeShown, recordToolModeShown} from '//resources/cr_components/composebox/common.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {ComposeboxContextAddedMethod, GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {DragAndDropHandler} from '//resources/cr_components/search/drag_drop_handler.js';
import type {DragAndDropHost} from '//resources/cr_components/search/drag_drop_host.js';
import {PlaceholderTextCycler} from '//resources/cr_components/searchbox/placeholder_text_cycler.js';
import {SearchboxElement} from '//resources/cr_components/searchbox/searchbox.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {waitForLazyRender} from '//resources/cr_components/searchbox/utils.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {InputType, ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

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

      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },

      animationState: {
        reflect: true,
        type: String,
      },

      //========================================================================
      // Protected properties
      //========================================================================
      tabSuggestions_: {type: Array},
      inputState_: {type: Object},
    };
  }

  accessor ntpRealboxNextEnabled: boolean = false;
  accessor composeboxEnabled: boolean = false;
  accessor composeButtonEnabled: boolean = false;
  accessor cyclingPlaceholders: boolean = false;
  accessor isDraggingFile: boolean = false;
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.INELIGIBLE;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor inputState_: InputState|null = null;
  protected dragAndDropHandler: DragAndDropHandler|null = null;
  private placeholderCycler_: PlaceholderTextCycler | null = null;
  private dragAndDropEnabled_: boolean =
      loadTimeData.getBoolean('composeboxContextDragAndDropEnabled');
  private onTabStripChangedListenerId_: number|null = null;
  private contextMenuOpened_: boolean = false;

  override async connectedCallback() {
    super.connectedCallback();

    if (this.ntpRealboxNextEnabled) {
      this.dragAndDropHandler =
          new DragAndDropHandler(this, this.dragAndDropEnabled_);
    }
    this.onTabStripChangedListenerId_ =
        this.callbackRouter_.onTabStripChanged.addListener(
            this.refreshTabSuggestions_.bind(this));
    this.inputState_ = (await this.pageHandler().getInputState()).state;
    if (this.inputState_) {
      this.inputState_.activeModel = ModelMode.kUnspecified;
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.placeholderCycler_?.stop();
    assert(this.onTabStripChangedListenerId_);
    this.callbackRouter_.removeListener(this.onTabStripChangedListenerId_);
  }

  override firstUpdated() {
    super.firstUpdated();

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler().getPlaceholderConfig();
        const texts = config.texts;
        if (texts.length < 2) {
          // Need at least 2 placeholders to cycle. If fewer, disable cycling
          // and let the static placeholder text show instead.
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

  protected onFileChange_(e: CustomEvent<{files: FileList}>) {
    this.processFiles_(
        e.detail.files, ComposeboxContextAddedMethod.CONTEXT_MENU);
  }

  protected onAddTabContext_(e: CustomEvent<{
    id: number,
    title: string,
    url: Url,
    delayUpload: boolean,
    origin: TabUploadOrigin,
  }>) {
    const attachment: TabUpload = {
      tabId: e.detail.id,
      url: e.detail.url,
      title: e.detail.title,
      delayUpload: e.detail.delayUpload,
      origin: e.detail.origin,
    };
    recordContextualElementClickedMetric(
        this.composeboxSource, 'ClassicPopup', ContextType.TAB);
    this.openComposebox_([attachment]);
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    // Only refresh tab suggestions if the context menu is opened.
    const requiresRefresh = forceRefresh || this.contextMenuOpened_;
    if (!requiresRefresh) {
      return;
    }
    const {tabs} = await this.pageHandler().getRecentTabs();
    this.tabSuggestions_ = [...tabs];

    if (this.contextMenuOpened_ && this.inputState_) {
      const {allowedInputTypes, disabledInputTypes} = this.inputState_;
      if (allowedInputTypes.includes(InputType.kBrowserTab) &&
          !disabledInputTypes.includes(InputType.kBrowserTab) &&
          this.tabSuggestions_.length > 0) {
        recordInputTypeShown(
            InputType.kBrowserTab, this.composeboxSource, 'ClassicPopup');
      }
    }
  }

  protected async onGetTabPreview_(e: CustomEvent<{
    tabId: number,
    onPreviewFetched: (previewDataUrl: string) => void,
  }>) {
    const {previewDataUrl} =
        await this.pageHandler().getTabPreview(e.detail.tabId);
    e.detail.onPreviewFetched(previewDataUrl || '');
  }

  protected onContextMenuClosed_() {
    this.contextMenuOpened_ = false;
    this.blur();
  }

  protected onContextMenuOpened_() {
    this.contextMenuOpened_ = true;
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);

    if (this.inputState_) {
      const {allowedInputTypes, disabledInputTypes} = this.inputState_;
      allowedInputTypes.forEach((inputType: InputType) => {
        // The `kBrowserTab` InputType requires special metrics handling as part
        // of `refreshTabSuggestions_()`.
        if (inputType !== InputType.kBrowserTab &&
            !disabledInputTypes.includes(inputType)) {
          recordInputTypeShown(
              inputType, this.composeboxSource, 'ClassicPopup');
        }
      });

      const {allowedTools, disabledTools} = this.inputState_;
      allowedTools.forEach((tool: ToolMode) => {
        if (!disabledTools.includes(tool)) {
          recordToolModeShown(tool, this.composeboxSource, 'ClassicPopup');
        }
      });

      const {allowedModels, disabledModels} = this.inputState_;
      allowedModels.forEach((model: ModelMode) => {
        if (!disabledModels.includes(model)) {
          recordModelModeShown(model, this.composeboxSource, 'ClassicPopup');
        }
      });
    }
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposebox_([], e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposebox_([], ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposebox_([], ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposebox_([], ToolMode.kUnspecified, e.detail.model);
  }

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
