// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './code_section.js';
import './shared_style.css.js';

import {CrContainerShadowMixin} from 'chrome://resources/cr_elements/cr_container_shadow_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './error_page.html.js';
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

const ExtensionsErrorPageElementBase = CrContainerShadowMixin(PolymerElement);

export class ExtensionsErrorPageElement extends ExtensionsErrorPageElementBase {
  static get is() {
    return 'extensions-error-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      data: Object,
      delegate: Object,

      // Whether or not dev mode is enabled.
      inDevMode: {
        type: Boolean,
        value: false,
        observer: 'onInDevModeChanged_',
      },

      entries_: Array,

      code_: Object,

      /**
       * Index into |entries_|.
       */
      selectedEntry_: {
        type: Number,
        observer: 'onSelectedErrorChanged_',
      },

      selectedStackFrame_: {
        type: Object,
        value() {
          return null;
        },
      },
    };
  }

  static get observers() {
    return ['observeDataChanges_(data.*)'];
  }

  data: chrome.developerPrivate.ExtensionInfo;
  delegate: ErrorPageDelegate;
  inDevMode: boolean;
  private entries_: Array<ManifestError|RuntimeError>;
  private code_: chrome.developerPrivate.RequestFileSourceResponse|null;
  private selectedEntry_: number;
  private selectedStackFrame_: chrome.developerPrivate.StackFrame|null;

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    FocusOutlineManager.forDocument(document);
  }

  getSelectedError(): ManifestError|RuntimeError {
    return this.entries_[this.selectedEntry_];
  }

  /**
   * Focuses the back button when page is loaded.
   */
  private onViewEnterStart_() {
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
    chrome.metricsPrivate.recordUserAction('Options_ViewExtensionErrors');
  }

  private getContextUrl_(error: ManifestError|RuntimeError, unknown: string):
      string {
    return (error as RuntimeError).contextUrl ?
        getRelativeUrl((error as RuntimeError).contextUrl, error) :
        unknown;
  }

  /**
   * Watches for changes to |data| in order to fetch the corresponding
   * file source.
   */
  private observeDataChanges_() {
    this.entries_ = [...this.data.manifestErrors, ...this.data.runtimeErrors];
    this.selectedEntry_ = -1;  // This also help reset code-section content.
    if (this.entries_.length) {
      this.selectedEntry_ = 0;
    }
  }

  private onCloseButtonTap_() {
    navigation.navigateTo({page: Page.LIST});
  }

  private onClearAllTap_() {
    const ids = this.entries_.map(entry => entry.id);
    this.delegate.deleteErrors(this.data.id, ids);
  }

  private computeErrorIcon_(error: ManifestError|RuntimeError): string {
    // Do not i18n these strings, they're CSS classes.
    return getErrorSeverityText(error, 'info', 'warning', 'error');
  }

  private computeErrorTypeLabel_(error: ManifestError|RuntimeError): string {
    return getErrorSeverityText(
        error, loadTimeData.getString('logLevel'),
        loadTimeData.getString('warnLevel'),
        loadTimeData.getString('errorLevel'));
  }

  private onDeleteErrorAction_(e: DomRepeatEvent<ManifestError|RuntimeError>) {
    this.delegate.deleteErrors(this.data.id, [e.model.item.id]);
    e.stopPropagation();
  }

  private onInDevModeChanged_() {
    if (!this.inDevMode) {
      // Wait until next render cycle in case error page is loading.
      setTimeout(() => {
        this.onCloseButtonTap_();
      }, 0);
    }
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
    this.delegate.requestFileSource(args).then(code => this.code_ = code);
  }

  private computeIsRuntimeError_(item: ManifestError|RuntimeError): boolean {
    return item.type === chrome.developerPrivate.ErrorType.RUNTIME;
  }

  /**
   * The description is a human-readable summation of the frame, in the
   * form "<relative_url>:<line_number> (function)", e.g.
   * "myfile.js:25 (myFunction)".
   */
  private getStackTraceLabel_(frame: chrome.developerPrivate.StackFrame):
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

  private getStackFrameClass_(frame: chrome.developerPrivate.StackFrame):
      string {
    return frame === this.selectedStackFrame_ ? 'selected' : '';
  }

  private getStackFrameTabIndex_(frame: chrome.developerPrivate.StackFrame):
      number {
    return frame === this.selectedStackFrame_ ? 0 : -1;
  }

  /**
   * This function is used to determine whether or not we want to show a
   * stack frame. We don't want to show code from internal scripts.
   */
  private shouldDisplayFrame_(url: string): boolean {
    // All our internal scripts are in the 'extensions::' namespace.
    return !/^extensions::/.test(url);
  }

  private updateSelected_(frame: chrome.developerPrivate.StackFrame) {
    this.selectedStackFrame_ = frame;

    const selectedError = this.getSelectedError();
    this.delegate
        .requestFileSource({
          extensionId: selectedError.extensionId,
          message: selectedError.message,
          pathSuffix: getRelativeUrl(frame.url, selectedError),
          lineNumber: frame.lineNumber,
        })
        .then(code => this.code_ = code);
  }

  private onStackFrameTap_(
      e: DomRepeatEvent<chrome.developerPrivate.StackFrame>) {
    const frame = e.model.item;
    this.updateSelected_(frame);
  }

  private onStackKeydown_(e: KeyboardEvent) {
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
        const repeaterEvent = e as unknown as DomRepeatEvent<RuntimeError>;
        const frame = repeaterEvent.model.item.stackTrace[i + direction];
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
  private computeErrorClass_(index: number): string {
    return index === this.selectedEntry_ ? 'selected' : '';
  }

  private iconName_(index: number): string {
    return index === this.selectedEntry_ ? 'icon-expand-less' :
                                           'icon-expand-more';
  }

  /**
   * Determine if the iron-collapse should be opened (expanded).
   */
  private isOpened_(index: number): boolean {
    return index === this.selectedEntry_;
  }


  /**
   * @return The aria-expanded value as a string.
   */
  private isAriaExpanded_(index: number): string {
    return this.isOpened_(index).toString();
  }

  private onErrorItemAction_(e: KeyboardEvent) {
    if (e.type === 'keydown' && !((e.code === 'Space' || e.code === 'Enter'))) {
      return;
    }

    // Call preventDefault() to avoid the browser scrolling when the space key
    // is pressed.
    e.preventDefault();
    const repeaterEvent =
        e as unknown as DomRepeatEvent<ManifestError|RuntimeError>;
    this.selectedEntry_ = this.selectedEntry_ === repeaterEvent.model.index ?
        -1 :
        repeaterEvent.model.index;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-error-page': ExtensionsErrorPageElement;
  }
}

customElements.define(
    ExtensionsErrorPageElement.is, ExtensionsErrorPageElement);
