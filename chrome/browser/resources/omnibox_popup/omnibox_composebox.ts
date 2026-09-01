// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/error_scrim.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/localized_link/localized_link.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_components/composebox/composebox_lens_search.js';
import {ComposeboxFile, getLoadTimeBoolean, hasOnlyAutoAddedTabs, hasOnlyAutoAddedTabAttachments, mapMojoSourceToOrigin, mapUploadErrorToProcessFilesError, ProcessFilesError} from '//resources/cr_components/composebox/common.js';
import type {TabUpload} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextUploadErrorType} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import {ContextUploadStatus} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {FileAttachment, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SearchContext, TabAttachment} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import {getCss} from './omnibox_composebox.css.js';
import {getHtml} from './omnibox_composebox.html.js';

export interface OmniboxComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
  };
}

export class OmniboxComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'cr-omnibox-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
      expanding_: {
        reflect: true,
        type: Boolean,
      },
      animationState: {
        type: String,
        reflect: true,
      },
      entrypointName: {type: String, reflect: true},
      enableCarouselScrolling: {type: Boolean},
      askGComposeboxLensChipEnabled_: {type: Boolean},
      // TODO(b/539981162): Consolidate the different lens buttons once
      // behaviors are solidified, and we can tell how they can be merged.
      isLensSearchChipShown_: {type: Boolean},
      isContentSharingEnabled_: {type: Boolean},
      isLensSearchEligible_: {type: Boolean},
      askGComposeboxPlaceholderEnabled_: {type: Boolean},
    };
  }

  accessor entrypointName: string = 'Omnibox';
  accessor applyContextButtonBackground: boolean = false;
  accessor enableCarouselScrolling: boolean = false;
  override accessor animationState: GlowAnimationState =
      GlowAnimationState.NONE;
  protected accessor expanding_: boolean = true;
  protected accessor askGComposeboxLensChipEnabled_: boolean =
      getLoadTimeBoolean('askGComposeboxLensChipEnabled', false);
  protected accessor isLensSearchChipShown_: boolean = false;
  protected accessor isContentSharingEnabled_: boolean = false;
  protected accessor isLensSearchEligible_: boolean = false;
  protected accessor askGComposeboxPlaceholderEnabled_: boolean =
      getLoadTimeBoolean('askGComposeboxPlaceholderEnabled', false);

  protected onLensSearchClick_(e: Event) {
    e.stopPropagation();
    this.getSearchboxHandler().openLensSearch();
  }

  // Reset Lens search eligibility when the popup is hidden. This prevents the
  // cached WebUI from briefly flashing the Lens chip from a previous eligible
  // page when the popup is reopened on an ineligible page (e.g. NTP).
  private onVisibilityChange_ = () => {
    if (document.visibilityState === 'hidden') {
      this.isLensSearchEligible_ = false;
    }
  };

  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;
  private listenerIds_: number[] = [];

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.animationState = GlowAnimationState.EXPANDING;
    this.updateInputPlaceholder();

    this.listenerIds_ = [
      this.searchboxCallbackRouter_.updateContentSharingPolicy.addListener(
          (enabled: boolean) => {
            this.isContentSharingEnabled_ = enabled;
          }),
      this.searchboxCallbackRouter_.updateLensSearchEligibility.addListener(
          (eligible: boolean) => {
            this.isLensSearchEligible_ = eligible;
          }),
    ];

    document.addEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    for (const listenerId of this.listenerIds_) {
      this.searchboxCallbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('webuiOmniboxSimplificationEnabled')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      this.applyContextButtonBackground =
          this.webuiOmniboxSimplificationEnabled && !inToolMode;
    }

    if (changedPrivateProperties.has('input') ||
        changedPrivateProperties.has('attachedContext') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('isContentSharingEnabled_') ||
        changedPrivateProperties.has('isLensSearchEligible_') ||
        changedPrivateProperties.has('askGComposeboxLensChipEnabled_')) {
      this.isLensSearchChipShown_ = this.askGComposeboxLensChipEnabled_ &&
          this.isContentSharingEnabled_ && this.isLensSearchEligible_ &&
          !this.hasContent(/* ignoreAutoAddedTabs= */ true);
    }

    if (changedPrivateProperties.has('attachedContext') ||
        changedPrivateProperties.has('inputState') ||
        changedPrivateProperties.has('askGComposeboxPlaceholderEnabled_')) {
      this.updateInputPlaceholder();
    }
  }

  override updateInputPlaceholder() {
    const hasAutoAddedTab = hasOnlyAutoAddedTabs(this.attachedContext);
    if (this.askGComposeboxPlaceholderEnabled_ && hasAutoAddedTab &&
        (!this.inputState ||
         this.inputState.activeTool === ToolMode.kUnspecified)) {
      this.inputPlaceholder = this.i18n('askAboutTab');
      return;
    }
    super.updateInputPlaceholder();
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
  }

  override deleteFile(uuidToDelete: UnguessableToken, fromUserAction?: boolean):
      ComposeboxFile|null {
    const file = super.deleteFile(uuidToDelete, fromUserAction);
    if (!file) {
      return null;
    }

    this.queryAutocomplete(/* clearMatches= */ true);
    return file;
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getContextEntrypointElement(): ContextualEntrypointButtonElement|
      null {
    return this.shadowRoot?.querySelector<ContextualEntrypointButtonElement>(
               '#contextEntrypoint') ||
        null;
  }


  override shouldShowDivider(): boolean {
    if (this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    return super.shouldShowDivider();
  }

  addSearchContext(context: SearchContext|null) {
    if (context) {
      if (context.input.length > 0) {
        this.input = context.input;
      }
      for (const attachment of context.attachments) {
        if (attachment.fileAttachment) {
          this.addFileFromAttachment_(attachment.fileAttachment);
        } else if (attachment.tabAttachment) {
          this.addTabFromAttachment_(attachment.tabAttachment);
        }
      }
    }

    const blockZeroState =
        getLoadTimeBoolean('askGBlockAutoTabZeroStateSuggestions', false);
    const hasOnlyAutoAdded =
        context ? hasOnlyAutoAddedTabAttachments(context.attachments) : false;
    const skipImmediateQuery = blockZeroState && hasOnlyAutoAdded;

    // Query for ZPS even if there's no context.
    // Skip immediate query if askGBlockAutoTabZeroStateSuggestions is enabled for
    // auto added tab attachments.
    // This is to prevent historical suggestions from being shown while waiting
    // for contextual suggestions from the uploaded tab during zero state.
    if (this.showZps && !skipImmediateQuery) {
      // Clear the autocomplete matches here, as failure to do so triggers a
      // DCHECK in `ZpsSection::InitMatches()` whenever the user tries to upload
      // a file after having uploaded an invalid file earlier in the session.
      this.queryAutocomplete(/* clearMatches= */ true);
    }
  }

  playGlowAnimation() {
    // If |animationState_| were still EXPANDING, this function would have no
    // effect because nothing changes in CSS and therefore animations wouldn't
    // be re-trigered. Resetting it to NONE forces the animation related styles
    // to reset before switching to EXPANDING.
    this.animationState = GlowAnimationState.NONE;
    // Wait for the style change for NONE to commit. This ensures the browser
    // detects a state change when we switch to EXPANDING.

    // If the composebox is not submittable, trigger the animation.
    if (!this.submitEnabled) {
      requestAnimationFrame(() => {
        this.animationState = GlowAnimationState.EXPANDING;
      });
    }
  }

  isExpanded(): boolean {
    return this.expanding_;
  }

  private addFileFromAttachment_(fileAttachment: FileAttachment) {
    const errorType = fileAttachment.errorType ?? null;
    if (errorType) {
      const processFilesError = mapUploadErrorToProcessFilesError(
          errorType as ContextUploadErrorType);
      if (processFilesError !== ProcessFilesError.NONE) {
        this.handleProcessFilesError(processFilesError);
        if (!super.deleteFile(fileAttachment.uuid)) {
          this.getSearchboxHandler().deleteContext(
              fileAttachment.uuid, /*fromAutomaticChip=*/ false);
        }
        return;
      }
    }

    const pendingStatus =
        this.attachedContext.get(fileAttachment.uuid)?.status;
    const composeboxFile = ComposeboxFile.createFromFile(
        fileAttachment.uuid,
        {name: fileAttachment.name, type: fileAttachment.mimeType},
        pendingStatus ?? ContextUploadStatus.kNotUploaded, {
          dataUrl: fileAttachment.imageDataUrl ?? null,
          iconUrl: fileAttachment.iconUrl,
          thumbnailUrl: fileAttachment.thumbnailUrl ?? null,
          supportsUnimodal: true,
        });
    this.onFileContextAdded(composeboxFile);
  }

  private addTabFromAttachment_(tabAttachment: TabAttachment) {
    this.addTabContextHandleCallback({
      tabId: tabAttachment.tabId,
      title: tabAttachment.title,
      url: tabAttachment.url,
      delayUpload: false,
      origin: mapMojoSourceToOrigin(tabAttachment.source),
    } as TabUpload);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-omnibox-composebox': OmniboxComposeboxElement;
  }
}

customElements.define(OmniboxComposeboxElement.is, OmniboxComposeboxElement);
