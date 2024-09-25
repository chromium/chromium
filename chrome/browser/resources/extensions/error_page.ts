// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './code_section.js';
import './shared_style.css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_page.css.js';
import {getHtml} from './error_page.html.js';
import type {ItemDelegate} from './item.js';
import {ItemMixinLit} from './item_mixin_lit.js';
import {navigation, Page} from './navigation_helper.js';

type ManifestError = chrome.developerPrivate.ManifestError;
type RuntimeError = chrome.developerPrivate.RuntimeError;

export interface ErrorPageDelegate {
  deleteErrors(
      extensionId: string, errorIds?: number[],
      type?: chrome.developerPrivate.ErrorType): void;

  requestFileSource(args: chrome.developerPrivate.RequestFileSourceProperties):
      Promise<chrome.developerPrivate.RequestFileSourceResponse>;
}

/**
 * Get the URL relative to the main extension url. If the url is
 * unassociated with the extension, this will be the full url.
 */
function getRelativeUrl(
    url: string, error: ManifestError|RuntimeError): string {
  const fullUrl = 'chrome-extension://' + error.extensionId + '/';
  return url.startsWith(fullUrl) ? url.substring(fullUrl.length) : url;
}

/**
 * Given 3 strings, this function returns the correct one for the type of
 * error that |item| is.
 */
function getErrorSeverityText(
    item: ManifestError|RuntimeError, log: string, warn: string,
    error: string): string {
  if (item.type === chrome.developerPrivate.ErrorType.RUNTIME) {
    switch ((item as RuntimeError).severity) {
      case chrome.developerPrivate.ErrorLevel.LOG:
        return log;
      case chrome.developerPrivate.ErrorLevel.WARN:
        return warn;
      case chrome.developerPrivate.ErrorLevel.ERROR:
        return error;
      default:
        assertNotReached();
    }
  }
  assert(item.type === chrome.developerPrivate.ErrorType.MANIFEST);
  return warn;
}

export interface ExtensionsErrorPageElement {
  $: {
    closeButton: HTMLElement,
  };
}

const ExtensionsErrorPageElementBase = ItemMixinLit(CrLitElement);

export class ExtensionsErrorPageElement extends ExtensionsErrorPageElementBase {
  static get is() {
    return 'extensions-error-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      data: {type: Object},
      delegate: {type: Object},

      // Whether or not dev mode is enabled.
      inDevMode: {type: Boolean},

      entries_: {type: Array},
      code_: {type: Object},

      /**
       * Index into |entries_|.
       */
      selectedEntry_: {type: Number},

      selectedStackFrame_: {type: Object},
    };
  }

  data?: chrome.developerPrivate.ExtensionInfo;
  delegate?: ErrorPageDelegate&ItemDelegate;
  inDevMode: boolean = false;
  protected entries_: Array<ManifestError|RuntimeError> = [];
  protected code_: chrome.developerPrivate.RequestFileSourceResponse|null =
      null;
  private selectedEntry_: number = -1;
  private selectedStackFrame_: chrome.developerPrivate.StackFrame|null = null;

  override firstUpdated() {
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    FocusOutlineManager.forDocument(document);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('data') && this.data) {
      /**
       * Watches for changes to |data| in order to fetch the corresponding
       * file source.
       */
      this.entries_ = [...this.data.manifestErrors, ...this.data.runtimeErrors];
      this.selectedEntry_ = this.entries_.length > 0 ? 0 : -1;
      this.onSelectedErrorChanged_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('inDevMode') && !this.inDevMode) {
      this.onCloseButtonClick_();
    }
  }

  getSelectedError(): ManifestError|RuntimeError {
    return this.entries_[this.selectedEntry_];
  }

  /**
   * Focuses the back button when page is loaded.
   */
  private onViewEnterStart_() {
    this.updateComplete.then(() => focusWithoutInk(this.$.closeButton));
    chrome.metricsPrivate.recordUserAction('Options_ViewExtensionErrors');
  }

  protected getContextUrl_(error: ManifestError|RuntimeError, unknown: string):
      string {
    return (error as RuntimeError).contextUrl ?
        getRelativeUrl((error as RuntimeError).contextUrl, error) :
        unknown;
  }

  protected onCloseButtonClick_() {
    navigation.navigateTo({page: Page.LIST});
  }

  protected onClearAllClick_() {
    const ids = this.entries_.map(entry => entry.id);
    assert(this.data);
    assert(this.delegate);
    this.delegate.deleteErrors(this.data.id, ids);
  }

  protected computeErrorIcon_(error: ManifestError|RuntimeError): string {
    // Do not i18n these strings, they're icon names.
    return getErrorSeverityText(error, 'cr:info', 'cr:warning', 'cr:error');
  }

  protected computeErrorTypeLabel_(error: ManifestError|RuntimeError): string {
    return getErrorSeverityText(
        error, loadTimeData.getString('logLevel'),
        loadTimeData.getString('warnLevel'),
        loadTimeData.getString('errorLevel'));
  }

  protected onDeleteErrorAction_(e: Event) {
    const id = Number((e.currentTarget as HTMLElement).dataset['errorId']);
    assert(this.data);
    assert(this.delegate);
    this.delegate.deleteErrors(this.data.id, [id]);
    e.stopPropagation();
  }

  /**
   * Fetches the source for the selected error and populates the code section.
   */
  private onSelectedErrorChanged_() {
    this.code_ = null;

    if (this.selectedEntry_ < 0) {
      return;
    }

    const error = this.getSelectedError();
    const args: chrome.developerPrivate.RequestFileSourceProperties = {
      extensionId: error.extensionId,
      message: error.message,
      pathSuffix: '',
    };
    switch (error.type) {
      case chrome.developerPrivate.ErrorType.MANIFEST:
        const manifestError = error as ManifestError;
        args.pathSuffix = manifestError.source;
        args.manifestKey = manifestError.manifestKey;
        args.manifestSpecific = manifestError.manifestSpecific;
        break;
      case chrome.developerPrivate.ErrorType.RUNTIME:
        const runtimeError = error as RuntimeError;
        try {
          // slice(1) because pathname starts with a /.
          args.pathSuffix = new URL(runtimeError.source).pathname.slice(1);
        } catch (e) {
          // Swallow the invalid URL error and return early. This prevents the
          // uncaught error from causing a runtime error as seen in
          // crbug.com/1257170.
          return;
        }
        args.lineNumber =
            runtimeError.stackTrace && runtimeError.stackTrace[0] ?
            runtimeError.stackTrace[0].lineNumber :
            0;
        this.selectedStackFrame_ =
            runtimeError.stackTrace && runtimeError.stackTrace[0] ?
            runtimeError.stackTrace[0] :
            null;
        break;
    }
    assert(this.delegate);
    this.delegate.requestFileSource(args).then(code => this.code_ = code);
  }

  protected computeIsRuntimeError_(item: ManifestError|RuntimeError): boolean {
    return item.type === chrome.developerPrivate.ErrorType.RUNTIME;
  }

  /**
   * The description is a human-readable summation of the frame, in the
   * form "<relative_url>:<line_number> (function)", e.g.
   * "myfile.js:25 (myFunction)".
   */
  protected getStackTraceLabel_(frame: chrome.developerPrivate.StackFrame):
      string {
    let description = getRelativeUrl(frame.url, this.getSelectedError()) + ':' +
        frame.lineNumber;

    if (frame.functionName) {
      const functionName = frame.functionName === '(anonymous function)' ?
          loadTimeData.getString('anonymousFunction') :
          frame.functionName;
      description += ' (' + functionName + ')';
    }

    return description;
  }

  protected getStackFrameClass_(frame: chrome.developerPrivate.StackFrame):
      string {
    return frame === this.selectedStackFrame_ ? 'selected' : '';
  }

  protected getStackFrameTabIndex_(frame: chrome.developerPrivate.StackFrame):
      number {
    return frame === this.selectedStackFrame_ ? 0 : -1;
  }

  /**
   * This function is used to determine whether or not we want to show a
   * stack frame. We don't want to show code from internal scripts.
   */
  protected shouldDisplayFrame_(url: string): boolean {
    // All our internal scripts are in the 'extensions::' namespace.
    return !/^extensions::/.test(url);
  }

  private updateSelected_(frame: chrome.developerPrivate.StackFrame) {
    this.selectedStackFrame_ = frame;

    const selectedError = this.getSelectedError();
    assert(this.delegate);
    this.delegate
        .requestFileSource({
          extensionId: selectedError.extensionId,
          message: selectedError.message,
          pathSuffix: getRelativeUrl(frame.url, selectedError),
          lineNumber: frame.lineNumber,
        })
        .then(code => this.code_ = code);
  }

  protected onStackFrameClick_(e: Event) {
    const target = e.currentTarget as HTMLElement;
    const frameIndex = Number(target.dataset['frameIndex']);
    const errorIndex = Number(target.dataset['errorIndex']);
    const error = this.entries_[errorIndex] as RuntimeError;
    const frame = error.stackTrace[frameIndex]!;
    this.updateSelected_(frame);
  }

  protected onStackKeydown_(e: KeyboardEvent) {
    let direction = 0;

    if (e.key === 'ArrowDown') {
      direction = 1;
    } else if (e.key === 'ArrowUp') {
      direction = -1;
    } else {
      return;
    }

    e.preventDefault();

    const list =
        (e.target as HTMLElement).parentElement!.querySelectorAll('li');

    for (let i = 0; i < list.length; ++i) {
      if (list[i].classList.contains('selected')) {
        const index =
            Number((e.currentTarget as HTMLElement).dataset['errorIndex']);
        const item = this.entries_[index] as RuntimeError;
        const frame = item.stackTrace[i + direction];
        if (frame) {
          this.updateSelected_(frame);
          list[i + direction].focus();  // Preserve focus.
        }
        return;
      }
    }
  }

  /**
   * Computes the class name for the error item depending on whether its
   * the currently selected error.
   */
  protected computeErrorClass_(index: number): string {
    return index === this.selectedEntry_ ? 'selected' : '';
  }

  protected iconName_(index: number): string {
    return index === this.selectedEntry_ ? 'icon-expand-less' :
                                           'icon-expand-more';
  }

  /**
   * Determine if the cr-collapse should be opened (expanded).
   */
  protected isOpened_(index: number): boolean {
    return index === this.selectedEntry_;
  }

  /**
   * @return The aria-expanded value as a string.
   */
  protected isAriaExpanded_(index: number): string {
    return this.isOpened_(index).toString();
  }

  protected onErrorItemAction_(e: KeyboardEvent) {
    if (e.type === 'keydown' && !((e.code === 'Space' || e.code === 'Enter'))) {
      return;
    }

    // Call preventDefault() to avoid the browser scrolling when the space key
    // is pressed.
    e.preventDefault();
    const index =
        Number((e.currentTarget as HTMLElement).dataset['errorIndex']);
    this.selectedEntry_ = this.selectedEntry_ === index ? -1 : index;
    this.onSelectedErrorChanged_();
  }

  protected showReloadButton_(): boolean {
    return this.canReloadItem();
  }

  protected onReloadClick_() {
    this.reloadItem().catch((loadError) => this.fire('load-error', loadError));
  }
}

// Exported to be used in the autogenerated Lit template file
export type ErrorPageElement = ExtensionsErrorPageElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-error-page': ExtensionsErrorPageElement;
  }
}

customElements.define(
    ExtensionsErrorPageElement.is, ExtensionsErrorPageElement);
