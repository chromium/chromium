// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//read-anything-side-panel.top-chrome/shared/sp_empty_state.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '../strings.m.js';
import './read_anything_toolbar.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {WebUiListenerMixin} from '//resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from '//resources/js/assert_ts.js';
import {rgbToSkColor, skColorToRgba} from '//resources/js/color_utils.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';

const ReadAnythingElementBase = WebUiListenerMixin(PolymerElement);

interface LinkColor {
  default: string;
  visited: string;
}
// TODO(crbug.com/1465029): Remove colors defined here once the Views toolbar is
// removed.
const style = getComputedStyle(document.body);
const darkThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-900-rgb'));
const lightThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-grey-50-rgb'));
const yellowThemeBackgroundSkColor =
    rgbToSkColor(style.getPropertyValue('--google-yellow-100-rgb'));
const darkThemeEmptyStateBodyColor = 'var(--google-grey-500)';
const defaultThemeEmptyStateBodyColor = 'var(--google-grey-700)';
const darkThemeLinkColors: LinkColor = {
  default: 'var(--google-blue-300)',
  visited: 'var(--google-purple-200)',
};
const defaultLinkColors: LinkColor = {
  default: 'var(--google-blue-900)',
  visited: 'var(--google-purple-900)',
};
const lightThemeLinkColors: LinkColor = {
  default: 'var(--google-blue-800)',
  visited: 'var(--google-purple-900)',
};
const darkThemeSelectionColor = 'var(--google-blue-200)';
const defaultSelectionColor = 'var(--google-yellow-100)';
const yellowThemeSelectionColor = 'var(--google-blue-100)';

// A two-way map where each key is unique and each value is unique. The keys are
// DOM nodes and the values are numbers, representing AXNodeIDs.
class TwoWayMap extends Map {
  #reverseMap;
  constructor() {
    super();
    this.#reverseMap = new Map();
  }
  override set(key: Node, value: number) {
    super.set(key, value);
    this.#reverseMap.set(value, key);
    return this;
  }
  keyFrom(value: number) {
    return this.#reverseMap.get(value);
  }
  override clear() {
    super.clear();
    this.#reverseMap.clear();
  }
}

/////////////////////////////////////////////////////////////////////
// Called by ReadAnythingUntrustedPageHandler via callback router. //
/////////////////////////////////////////////////////////////////////

// The chrome.readingMode context is created by the ReadAnythingAppController
// which is only instantiated when the kReadAnything feature is enabled. This
// check if chrome.readingMode exists prevents runtime errors when the feature
// is disabled.
if (chrome.readingMode) {
  chrome.readingMode.updateContent = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateContent();
  };

  chrome.readingMode.updateSelection = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateSelection();
  };

  chrome.readingMode.updateTheme = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.updateTheme();
  };

  chrome.readingMode.showLoading = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.showLoading();
  };

  chrome.readingMode.showEmpty = () => {
    const readAnythingApp = document.querySelector('read-anything-app');
    assert(readAnythingApp);
    readAnythingApp.showEmpty();
  };
}

export class ReadAnythingElement extends ReadAnythingElementBase {
  static get is() {
    return 'read-anything-app';
  }

  static get template() {
    return getTemplate();
  }

  // Defines the valid font names that can be passed to front-end and maps
  // them to a corresponding class style in app.html. Must stay in-sync with
  // the names set in read_anything_font_model.cc.
  private defaultFontName_: string = 'sans-serif';
  private validFontNames_: Array<{name: string, css: string}> = [
    {name: 'Poppins', css: 'Poppins'},
    {name: 'Sans-serif', css: 'sans-serif'},
    {name: 'Serif', css: 'serif'},
    {name: 'Comic Neue', css: '"Comic Neue"'},
    {name: 'Lexend Deca', css: '"Lexend Deca"'},
    {name: 'EB Garamond', css: '"EB Garamond"'},
    {name: 'STIX Two Text', css: '"STIX Two Text"'},
  ];

  // Maps a DOM node to the AXNodeID that was used to create it. DOM nodes and
  // AXNodeIDs are unique, so this is a two way map where either DOM node or
  // AXNodeID can be used to access the other.
  private domNodeToAxNodeIdMap_: TwoWayMap = new TwoWayMap();

  private scrollingOnSelection_: boolean;
  private hasContent_: boolean;
  private emptyStateImagePath_: string;
  private emptyStateDarkImagePath_: string;
  private emptyStateHeading_: string;
  private emptyStateSubheading_: string;

  // If the WebUI toolbar should be shown. This happens when the WebUI feature
  // flag is enabled.
  private isWebUIToolbarVisible_: boolean;

  constructor() {
    super();
    if (chrome.readingMode && chrome.readingMode.isWebUIToolbarVisible) {
      ColorChangeUpdater.forDocument().start();
    }
  }

  override connectedCallback() {
    super.connectedCallback();
    if (chrome.readingMode) {
      chrome.readingMode.onConnected();
    }

    this.showLoading();

    document.onselectionchange = () => {
      if (!this.hasContent_) {
        return;
      }
      const shadowRoot = this.shadowRoot;
      assert(shadowRoot);
      const selection = shadowRoot.getSelection();
      assert(selection);
      const {anchorNode, anchorOffset, focusNode, focusOffset} = selection;
      if (!anchorNode || !focusNode) {
        return;
      }
      const anchorNodeId = this.domNodeToAxNodeIdMap_.get(anchorNode);
      const focusNodeId = this.domNodeToAxNodeIdMap_.get(focusNode);
      assert(anchorNodeId && focusNodeId);
      chrome.readingMode.onSelectionChange(
          anchorNodeId, anchorOffset, focusNodeId, focusOffset);
    };

    document.onscroll = () => {
      chrome.readingMode.onScroll(this.scrollingOnSelection_);
      this.scrollingOnSelection_ = false;
    };

    // Pass copy commands to main page. Copy commands will not work if they are
    // disabled on the main page.
    document.oncopy = () => {
      chrome.readingMode.onCopy();
      return false;
    };

    this.isWebUIToolbarVisible_ = chrome.readingMode.isWebUIToolbarVisible;
  }

  private buildSubtree_(nodeId: number): Node {
    let htmlTag = chrome.readingMode.getHtmlTag(nodeId);

    // Text nodes do not have an html tag.
    if (!htmlTag.length) {
      return this.createTextNode_(nodeId);
    }

    // getHtmlTag might return '#document' which is not a valid to pass to
    // createElement.
    if (htmlTag === '#document') {
      htmlTag = 'div';
    }

    const element = document.createElement(htmlTag);
    this.domNodeToAxNodeIdMap_.set(element, nodeId);
    const direction = chrome.readingMode.getTextDirection(nodeId);
    if (direction) {
      element.setAttribute('dir', direction);
    }
    const url = chrome.readingMode.getUrl(nodeId);
    if (url && element.nodeName === 'A') {
      element.setAttribute('href', url);
      element.onclick = () => {
        chrome.readingMode.onLinkClicked(nodeId);
      };
    }
    const language = chrome.readingMode.getLanguage(nodeId);
    if (language) {
      element.setAttribute('lang', language);
    }

    this.appendChildSubtrees_(element, nodeId);
    return element;
  }

  private appendChildSubtrees_(node: Node, nodeId: number) {
    for (const childNodeId of chrome.readingMode.getChildren(nodeId)) {
      const childNode = this.buildSubtree_(childNodeId);
      node.appendChild(childNode);
    }
  }

  private createTextNode_(nodeId: number): Node {
    const textContent = chrome.readingMode.getTextContent(nodeId);
    const textNode = document.createTextNode(textContent);
    this.domNodeToAxNodeIdMap_.set(textNode, nodeId);
    const shouldBold = chrome.readingMode.shouldBold(nodeId);
    const isOverline = chrome.readingMode.isOverline(nodeId);

    if (!shouldBold && !isOverline) {
      return textNode;
    }

    const htmlTag = shouldBold ? 'b' : 'span';
    const parentElement = document.createElement(htmlTag);
    if (isOverline) {
      parentElement.style.textDecoration = 'overline';
    }
    parentElement.appendChild(textNode);
    return parentElement;
  }

  showEmpty() {
    if (chrome.readingMode.isSelectable) {
      this.emptyStateHeading_ = loadTimeData.getString('emptyStateHeader');
    } else {
      this.emptyStateHeading_ = loadTimeData.getString('notSelectableHeader');
    }
    this.emptyStateImagePath_ = './images/empty_state.svg';
    this.emptyStateDarkImagePath_ = './images/empty_state.svg';
    this.emptyStateSubheading_ = loadTimeData.getString('emptyStateSubheader');
    this.hasContent_ = false;
  }

  showLoading() {
    this.emptyStateImagePath_ = '//resources/images/throbber_small.svg';
    this.emptyStateDarkImagePath_ =
        '//resources/images/throbber_small_dark.svg';
    this.emptyStateHeading_ =
        loadTimeData.getString('readAnythingLoadingMessage');
    this.emptyStateSubheading_ = '';
    this.hasContent_ = false;
  }

  updateContent() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const container = shadowRoot.getElementById('container');
    assert(container);

    // Remove all children from container. Use `replaceChildren` rather than
    // setting `innerHTML = ''` in order to remove all listeners, too.
    container.replaceChildren();
    this.domNodeToAxNodeIdMap_.clear();

    // Construct a dom subtree starting with the display root and append it to
    // the container. The display root may be invalid if there are no content
    // nodes and no selection.
    // This does not use polymer's templating abstraction, which
    // would create a shadow node element representing each AXNode, because
    // experimentation found the shadow node creation to be ~8-10x slower than
    // constructing and appending nodes directly to the container element.
    const rootId = chrome.readingMode.rootId;
    if (!rootId) {
      return;
    }

    const node = this.buildSubtree_(rootId);
    if (!node.textContent) {
      return;
    }

    this.hasContent_ = true;
    container.appendChild(node);
  }

  updateSelection() {
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const selection = shadowRoot.getSelection();
    assert(selection);
    selection.removeAllRanges();

    const range = new Range();
    const startNodeId = chrome.readingMode.startNodeId;
    const startOffset = chrome.readingMode.startOffset;
    const endNodeId = chrome.readingMode.endNodeId;
    const endOffset = chrome.readingMode.endOffset;
    const startNode = this.domNodeToAxNodeIdMap_.keyFrom(startNodeId);
    const endNode = this.domNodeToAxNodeIdMap_.keyFrom(endNodeId);
    if (!startNode || !endNode) {
      return;
    }
    range.setStart(startNode, startOffset);
    range.setEnd(endNode, endOffset);
    selection.addRange(range);

    // Scroll the start node into view. ScrollIntoView is available on the
    // Element class.
    const startElement = startNode.nodeType === Node.ELEMENT_NODE ?
        startNode as Element :
        startNode.parentElement;
    if (!startElement) {
      return;
    }
    this.scrollingOnSelection_ = true;
    startElement.scrollIntoViewIfNeeded();
  }

  // TODO(b/1465029): Once the IsReadAnythingWebUIEnabled flag is removed
  // this should be renamed to just validatedFontName_ and the current
  // validatedFontName_ method can be removed.
  private validatedFontNameFromName_(fontName: string): string {
    // Validate that the given font name is a valid choice, or use the default.
    const validFontName =
        this.validFontNames_.find((f: {name: string}) => f.name === fontName);
    return validFontName ? validFontName.css : this.defaultFontName_;
  }

  private validatedFontName_(): string {
    return this.validatedFontNameFromName_(chrome.readingMode.fontName);
  }

  private getLinkColor_(backgroundSkColor: SkColor): LinkColor {
    switch (backgroundSkColor.value) {
      case darkThemeBackgroundSkColor.value:
        return darkThemeLinkColors;
      case lightThemeBackgroundSkColor.value:
        return lightThemeLinkColors;
      default:
        return defaultLinkColors;
    }
  }

  private getEmptyStateBodyColor_(backgroundSkColor: SkColor): string {
    const isDark = backgroundSkColor.value === darkThemeBackgroundSkColor.value;
    return isDark ? darkThemeEmptyStateBodyColor :
                    defaultThemeEmptyStateBodyColor;
  }

  // TODO(crbug.com/1465029): This method should be renamed to
  // getEmptyStateBodyColor_() and replace the one above once we've removed the
  // Views toolbar.
  private getEmptyStateBodyColorFromWebUi_(colorSuffix: string): string {
    const isDark = colorSuffix.includes('dark');
    return isDark ? darkThemeEmptyStateBodyColor :
                    defaultThemeEmptyStateBodyColor;
  }

  private getSelectionColor_(backgroundSkColor: SkColor): string {
    switch (backgroundSkColor.value) {
      case darkThemeBackgroundSkColor.value:
        return darkThemeSelectionColor;
      case yellowThemeBackgroundSkColor.value:
        return yellowThemeSelectionColor;
      default:
        return defaultSelectionColor;
    }
  }

  updateLineSpacing(newLineHeight: string) {
    this.updateStyles({
      '--line-height': newLineHeight,
    });
  }

  updateLetterSpacing(newLetterSpacing: string) {
    this.updateStyles({
      '--letter-spacing': newLetterSpacing + 'em',
    });
  }

  updateFont(fontName: string) {
    const validatedFontName = this.validatedFontNameFromName_(fontName);
    this.updateStyles({
      '--font-family': validatedFontName,
    });

    // Also update the font on the toolbar itself with the validated font name.
    // TODO(crbug.com/1465029): Ensure the toolbar font is persisted to prefs.
    const shadowRoot = this.shadowRoot;
    assert(shadowRoot);
    const toolbar = shadowRoot.getElementById('toolbar');
    if (toolbar) {
      toolbar.style.fontFamily = validatedFontName;
    }
  }

  // TODO(crbug.com/1465029): This method should be renamed to updateTheme()
  // and replace the one below once we've removed the Views toolbar.
  updateThemeFromWebUi(colorSuffix: string) {
    const emptyStateBodyColor = colorSuffix ?
        this.getEmptyStateBodyColorFromWebUi_(colorSuffix) :
        'var(--color-side-panel-card-secondary-foreground)';
    this.updateStyles({
      '--background-color':
          `var(--color-read-anything-background${colorSuffix})`,
      '--foreground-color':
          `var(--color-read-anything-foreground${colorSuffix})`,
      '--sp-empty-state-heading-color':
          `var(--color-read-anything-foreground${colorSuffix})`,
      '--sp-empty-state-body-color': emptyStateBodyColor,
      '--selection-color':
          `var(--color-read-anything-text-selection${colorSuffix})`,
      '--link-color': `var(--color-read-anything-link-default${colorSuffix})`,
      '--visited-link-color':
          `var(--color-read-anything-link-visited${colorSuffix})`,
    });
  }

  updateTheme() {
    const foregroundColor:
        SkColor = {value: chrome.readingMode.foregroundColor};
    const backgroundColor:
        SkColor = {value: chrome.readingMode.backgroundColor};
    const linkColor = this.getLinkColor_(backgroundColor);

    this.updateStyles({
      '--background-color': skColorToRgba(backgroundColor),
      '--font-family': this.validatedFontName_(),
      '--font-size': chrome.readingMode.fontSize + 'em',
      '--foreground-color': skColorToRgba(foregroundColor),
      '--letter-spacing': chrome.readingMode.letterSpacing + 'em',
      '--line-height': chrome.readingMode.lineSpacing,
      '--link-color': linkColor.default,
      '--selection-color': this.getSelectionColor_(backgroundColor),
      '--sp-empty-state-heading-color': skColorToRgba(foregroundColor),
      '--sp-empty-state-body-color':
          this.getEmptyStateBodyColor_(backgroundColor),
      '--visited-link-color': linkColor.visited,
    });
    if (!chrome.readingMode.isWebUIToolbarVisible) {
      document.body.style.background = skColorToRgba(backgroundColor);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'read-anything-app': ReadAnythingElement;
  }
}

customElements.define(ReadAnythingElement.is, ReadAnythingElement);
