// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.automation API. */
// TODO(crbug.com/1203307): Auto-generate this file.

declare namespace chrome {
  export namespace automation {
    export enum EventType {
      ACCESS_KEY_CHANGED = 'accessKeyChanged',
      ACTIVE_DESCENDANT_CHANGED = 'activeDescendantChanged',
      ALERT = 'alert',
      // TODO(crbug.com/1464633) Fully remove ARIA_ATTRIBUTE_CHANGED_DEPRECATED
      // starting in 122, because although it was removed in 118, it is still
      // present in earlier versions of LaCros.
      ARIA_ATTRIBUTE_CHANGED_DEPRECATED = 'ariaAttributeChangedDeprecated',
      ARIA_CURRENT_CHANGED = 'ariaCurrentChanged',
      ATOMIC_CHANGED = 'atomicChanged',
      AUTO_COMPLETE_CHANGED = 'autoCompleteChanged',
      AUTOCORRECTION_OCCURED = 'autocorrectionOccured',
      AUTOFILL_AVAILABILITY_CHANGED = 'autofillAvailabilityChanged',
      BLUR = 'blur',
      BUSY_CHANGED = 'busyChanged',
      CARET_BOUNDS_CHANGED = 'caretBoundsChanged',
      CHECKED_STATE_CHANGED = 'checkedStateChanged',
      CHECKED_STATE_DESCRIPTION_CHANGED = 'checkedStateDescriptionChanged',
      CHILDREN_CHANGED = 'childrenChanged',
      CLASS_NAME_CHANGED = 'classNameChanged',
      CLICKED = 'clicked',
      COLLAPSED = 'collapsed',
      CONTROLS_CHANGED = 'controlsChanged',
      DETAILS_CHANGED = 'detailsChanged',
      DESCRIBED_BY_CHANGED = 'describedByChanged',
      DESCRIPTION_CHANGED = 'descriptionChanged',
      DOCUMENT_SELECTION_CHANGED = 'documentSelectionChanged',
      DOCUMENT_TITLE_CHANGED = 'documentTitleChanged',
      DROPEFFECT_CHANGED = 'dropeffectChanged',
      EDITABLE_TEXT_CHANGED = 'editableTextChanged',
      ENABLED_CHANGED = 'enabledChanged',
      END_OF_TEST = 'endOfTest',
      EXPANDED = 'expanded',
      EXPANDED_CHANGED = 'expandedChanged',
      FLOW_FROM_CHANGED = 'flowFromChanged',
      FLOW_TO_CHANGED = 'flowToChanged',
      FOCUS = 'focus',
      FOCUS_AFTER_MENU_CLOSE = 'focusAfterMenuClose',
      FOCUS_CHANGED = 'focusChanged',
      FOCUS_CONTEXT = 'focusContext',
      GRABBED_CHANGED = 'grabbedChanged',
      HASPOPUP_CHANGED = 'haspopupChanged',
      HIDE = 'hide',
      HIERARCHICAL_LEVEL_CHANGED = 'hierarchicalLevelChanged',
      HIT_TEST_RESULT = 'hitTestResult',
      HOVER = 'hover',
      IGNORED_CHANGED = 'ignoredChanged',
      IMAGE_ANNOTATION_CHANGED = 'imageAnnotationChanged',
      IMAGE_FRAME_UPDATED = 'imageFrameUpdated',
      INVALID_STATUS_CHANGED = 'invalidStatusChanged',
      KEY_SHORTCUTS_CHANGED = 'keyShortcutsChanged',
      LABELED_BY_CHANGED = 'labeledByChanged',
      LANGUAGE_CHANGED = 'languageChanged',
      LAYOUT_COMPLETE = 'layoutComplete',
      LAYOUT_INVALIDATED = 'layoutInvalidated',
      LIVE_REGION_CHANGED = 'liveRegionChanged',
      LIVE_REGION_CREATED = 'liveRegionCreated',
      LIVE_REGION_NODE_CHANGED = 'liveRegionNodeChanged',
      LIVE_RELEVANT_CHANGED = 'liveRelevantChanged',
      LIVE_STATUS_CHANGED = 'liveStatusChanged',
      LOAD_COMPLETE = 'loadComplete',
      LOAD_START = 'loadStart',
      LOCATION_CHANGED = 'locationChanged',
      MEDIA_STARTED_PLAYING = 'mediaStartedPlaying',
      MEDIA_STOPPED_PLAYING = 'mediaStoppedPlaying',
      MENU_END = 'menuEnd',
      MENU_ITEM_SELECTED = 'menuItemSelected',
      MENU_LIST_VALUE_CHANGED = 'menuListValueChanged',
      MENU_POPUP_END = 'menuPopupEnd',
      MENU_POPUP_START = 'menuPopupStart',
      MENU_START = 'menuStart',
      MOUSE_CANCELED = 'mouseCanceled',
      MOUSE_DRAGGED = 'mouseDragged',
      MOUSE_MOVED = 'mouseMoved',
      MOUSE_PRESSED = 'mousePressed',
      MOUSE_RELEASED = 'mouseReleased',
      MULTILINE_STATE_CHANGED = 'multilineStateChanged',
      MULTISELECTABLE_STATE_CHANGED = 'multiselectableStateChanged',
      NAME_CHANGED = 'nameChanged',
      OBJECT_ATTRIBUTE_CHANGED = 'objectAttributeChanged',
      OTHER_ATTRIBUTE_CHANGED = 'otherAttributeChanged',
      PARENT_CHANGED = 'parentChanged',
      PLACEHOLDER_CHANGED = 'placeholderChanged',
      PORTAL_ACTIVATED = 'portalActivated',
      POSITION_IN_SET = 'positionInSet',
      RANGE_VALUE_CHANGED = 'rangeValueChanged',
      RANGE_VALUE_MAX_CHANGED = 'rangeValueMaxChanged',
      RANGE_VALUE_MIN_CHANGED = 'rangeValueMinChanged',
      RANGE_VALUE_STEP_CHANGED = 'rangeValueStepChanged',
      READONLY_CHANGED = 'readonlyChanged',
      RELATED_NODE_CHANGED = 'relatedNodeChanged',
      REQUIRED_STATE_CHANGED = 'requiredStateChanged',
      ROLE_CHANGED = 'roleChanged',
      ROW_COLLAPSED = 'rowCollapsed',
      ROW_COUNT_CHANGED = 'rowCountChanged',
      ROW_EXPANDED = 'rowExpanded',
      SCROLL_HORIZONTAL_POSITION_CHANGED = 'scrollHorizontalPositionChanged',
      SCROLL_POSITION_CHANGED = 'scrollPositionChanged',
      SCROLL_VERTICAL_POSITION_CHANGED = 'scrollVerticalPositionChanged',
      SCOLLED_TO_ANCHOR = 'scrollToAnchor',
      SELECTED_CHANGED = 'selectedChanged',
      SELECTED_CHILDREN_CHANGED = 'selectedChildrenChanged',
      SELECTED_VALUE_CHANGED = 'selectedValueChanged',
      SELECTION = 'selection',
      SELECTION_ADD = 'selectionAdd',
      SELECTION_REMOVE = 'selectionRemove',
      SET_SIZE_CHANGED = 'setSizeChange',
      SHOW = 'show',
      SORT_CHANGED = 'sortChanged',
      STATE_CHANGED = 'stateChanged',
      SUBTREE_CHANGED = 'subtreeChanged',
      TEXT_ATTRIBUTE_CHANGED = 'textAttributeChanged',
      TEXT_SELECTION_CHANGED = 'textSelectionChanged',
      TEXT_CHANGED = 'textChanged',
      TOOLTIP_CLOSED = 'tooltipClosed',
      TOOLTIP_OPENED = 'tooltipOpened',
      TREE_CHANGED = 'treeChanged',
      VALUE_IN_TEXT_FIELD_CHANGED = 'valueInTextFieldChanged',
      VALUE_CHANGED = 'valueChanged',
      WINDOW_ACTIVATED = 'windowActivated',
      WINDOW_DEACTIVATED = 'windowDeactivated',
      WINDOW_VISIBILITY_CHANGED = 'windowVisibilityChanged'
    }

    export enum RoleType {
      ABBR = 'abbr',
      ALERT = 'alert',
      ALERT_DIALOG = 'alertDialog',
      APPLICATION = 'application',
      ARTICLE = 'article',
      AUDIO = 'audio',
      BANNER = 'banner',
      BLOCKQUOTE = 'blockquote',
      BUTTON = 'button',
      CANVAS = 'canvas',
      CAPTION = 'caption',
      CARET = 'caret',
      CELL = 'cell',
      CHECK_BOX = 'checkBox',
      CLIENT = 'client',
      CODE = 'code',
      COLOR_WELL = 'colorWell',
      COLUMN = 'column',
      COLUMN_HEADER = 'columnHeader',
      COMBO_BOX_GROUPING = 'comboBoxGrouping',
      COMBO_BOX_MENU_BUTTON = 'comboBoxMenuButton',
      COMBO_BOX_SELECT = 'comboBoxSelect',
      COMMENT = 'comment',
      COMPLEMENTARY = 'complementary',
      CONTENT_DELETION = 'contentDeletion',
      CONTENT_INSERTION = 'contentInsertion',
      CONTENT_INFO = 'contentInfo',
      DATE = 'date',
      DATE_TIME = 'dateTime',
      DEFINITION = 'definition',
      DESCRIPTION_LIST = 'descriptionList',
      DESCRIPTION_LIST_DETAIL = 'descriptionListDetail',
      DESCRIPTION_LIST_TERM = 'descriptionListTerm',
      DESKTOP = 'desktop',
      DETAILS = 'details',
      DIALOG = 'dialog',
      DIRECTORY = 'directory',
      DISCLOSURE_TRIANGLE = 'disclosure_triangle',
      DOC_ABSTRACT = 'docAbstract',
      DOC_ACKNOWLEDGEMENTS = 'docAcknowledgements',
      DOC_AFTERWARD = 'docAfterward',
      DOC_APPENDIX = 'docAppendix',
      DOC_BACK_LINK = 'docBackLink',
      DOC_BIBLIO_ENTRY = 'docBiblioEntry',
      DOC_BIBLIOGRAPHY = 'docBibliography',
      DOC_BIBLIO_REF = 'docBiblioRef',
      DOC_CHAPTER = 'docChapter',
      DOC_COLOPHON = 'docColophon',
      DOC_CONCLUSION = 'docConclusion',
      DOC_COVER = 'docCover',
      DOC_CREDIT = 'docCredit',
      DOC_CREDITS = 'docCredits',
      DOC_DEDICATION = 'docDedication',
      DOC_ENDNOTE = 'docEndnote',
      DOC_ENDNOTES = 'docEndnotes',
      DOC_EPIGRAPH = 'docEpigraph',
      DOC_EPILOGUE = 'docEpilogue',
      DOC_ERRATA = 'docErrata',
      DOC_EXAMPLE = 'docExample',
      DOC_FOOTNOTE = 'docFootnote',
      DOC_FOREWORD = 'docForeword',
      DOC_GLOSSARY = 'docGlossary',
      DOC_GLOSS_REF = 'docGlossRef',
      DOC_INDEX = 'docIndex',
      DOC_INTRODUCTION = 'docIntroduction',
      DOC_NOTE_REF = 'docNoteRef',
      DOC_NOTICE = 'docNotice',
      DOC_PAGE_BREAK = 'docPageBreak',
      DOC_PAGE_FOOTER = 'docPageFooter',
      DOC_PAGE_HEADER = 'docPageHeader',
      DOC_PAGE_LIST = 'docPageList',
      DOC_PART = 'docPart',
      DOC_PREFACE = 'docPreface',
      DOC_PROLOGUE = 'docPrologue',
      DOC_PULLQUOTE = 'docPullquote',
      DOC_QNA = 'docQuna',
      DOC_SUBTITLE = 'docSubtitle',
      DOC_TIP = 'docTip',
      DOC_TOC = 'docToc',
      DOCUMENT = 'document',
      EMBEDDED_DOCUMENT = 'embeddedDocument',
      EMPHASIS = 'emphasis',
      FEED = 'feed',
      FIGCAPTION = 'figcaption',
      FIGURE = 'figure',
      FOOTER = 'footer',
      FOTTER_AS_NON_LANDMARK = 'footerAsNonLandmark',
      FORM = 'form',
      GENERIC_CONTAINER = 'genericContainer',
      GRAPHICS_DOCUMENT = 'graphicsDocument',
      GRAPHICS_OBJECT = 'graphicsObject',
      GRAPHICS_SYMBOL = 'graphicsSymbol',
      GRID = 'grid',
      GROUP = 'group',
      HEADER = 'header',
      HEADER_AS_NON_LANDMARK = 'headerAsNonLandmark',
      HEADING = 'heading',
      IFRAME = 'iframe',
      IFRAME_PRESENTATIONAL = 'iframePresentational',
      IMAGE = 'image',
      IME_CANDIDATE = 'imeCandidate',
      INLINE_TEXT_BOX = 'inlineTextBox',
      INPUT_TIME = 'inputTime',
      KEYBOARD = 'keyboard',
      LABEL_TEXT = 'labelText',
      LAYOUT_TABLE = 'layoutTable',
      LAYOUT_TABLE_CELL = 'layoutTableCell',
      LAYOUT_TABLE_ROW = 'layoutTableRow',
      LEGEND = 'legend',
      LINE_BREAK = 'lineBreak',
      LINK = 'link',
      LIST = 'list',
      LIST_BOX = 'listBox',
      LIST_BOX_OPTION = 'listBoxOption',
      LIST_GRID = 'listGrid',
      LIST_ITEM = 'listItem',
      LIST_MARKER = 'listMarker',
      LOG = 'log',
      MAIN = 'main',
      MARK = 'mark',
      MARQUEE = 'marquee',
      MATH = 'math',
      MATH_ML_FRACTION = 'mathMLFraction',
      MATH_ML_IDENTIFIER = 'mathMLIdentifier',
      MATH_ML_MATH = 'mathMLMath',
      MATH_ML_MULTISCRIPTS = 'mathMLMultiscripts',
      MATH_ML_NONE_SCRIPT = 'mathMLNoneScript',
      MATH_ML_NUMBER = 'mathMLNumber',
      MATH_ML_OPERATOR = 'mathMLOperator',
      MATH_ML_OVER = 'mathMLOver',
      MATH_ML_PRESCRIPT_DELIMITER = 'mathMLPrescriptDelimiter',
      MATH_ML_ROOT = 'mathMLRoot',
      MATH_ML_ROW = 'mathMLRow',
      MATH_ML_SQUARE_ROOT = 'mathMLSquareRoot',
      MATH_ML_STRING_LITERAL = 'mathMLStringLiteral',
      MATH_ML_SUB = 'mathMLSub',
      MATH_ML_SUB_SUP = 'mathMLSubSup',
      MATH_ML_SUP = 'mathMLSup',
      MATH_ML_TABLE = 'mathMLTable',
      MATH_ML_TABLE_CELL = 'mathMLTableCell',
      MATH_ML_TABLE_ROW = 'mathMLTableRow',
      MATH_ML_TEXT = 'mathMLText',
      MATH_ML_UNDER = 'mathMLUnder',
      MATH_ML_UNDER_OVER = 'mathMLUnderOver',
      MENU = 'menu',
      MENU_BAR = 'menuBar',
      MENU_ITEM = 'menuItem',
      MENU_ITEM_CHECK_BOX = 'menuItemCheckBox',
      MENU_ITEM_RADIO = 'menuItemRadio',
      MENU_LIST_OPTION = 'menuListOption',
      MENU_LIST_POPUP = 'menuListPopup',
      METER = 'meter',
      NAVIGATION = 'navigation',
      NOTE = 'note',
      PANE = 'pane',
      PARAGRAPH = 'paragraph',
      PDF_ACTIONABLE_HIGHLIGHT = 'pdfActionableHighlight',
      PDF_ROOT = 'pdfRoot',
      PLUGIN_OBJECT = 'pluginObject',
      POP_UP_BUTTON = 'popUpButton',
      PORTAL = 'portal',
      PRE_DEPRECATED = 'preDeprecated',
      PROGRESS_INDICATOR = 'progressIndicator',
      RADIO_BUTTON = 'radioButton',
      RADIO_GROUP = 'radioGroup',
      REGION = 'region',
      ROOT_WEB_AREA = 'rootWebArea',
      ROW = 'row',
      ROW_GROUP = 'rowGroup',
      ROW_HEADER = 'rowHeader',
      RUBY = 'ruby',
      RUBY_ANNOTATION = 'rubyAnnotation',
      SCROLL_BAR = 'scrollBar',
      SCROLL_VIEW = 'scrollView',
      SEARCH = 'search',
      SEARCH_BOX = 'searchBox',
      SECTION = 'section',
      SLIDER = 'slider',
      SPIN_BUTTON = 'spinButton',
      SPLITTER = 'splitter',
      STATIC_TEXT = 'staticText',
      STATUS = 'status',
      STRONG = 'strong',
      SUBSCRIPT = 'subscript',
      SUGGESTION = 'suggestion',
      SUPERSCRIPT = 'superscript',
      SVG_ROOT = 'svgRoot',
      SWITCH = 'switch',
      TAB = 'tab',
      TAB_LIST = 'tabList',
      TAB_PANEL = 'tabPanel',
      TABLE = 'table',
      TABLE_HEADER_CONTAINER = 'tableHeaderContainer',
      TERM = 'term',
      TEXT_FIELD = 'textField',
      TEXT_FIELD_WITH_COMBO_BOX = 'textFieldWithComboBox',
      TIME = 'time',
      TIMER = 'timer',
      TITLE_BAR = 'titleBar',
      TOGGLE_BUTTON = 'toggleButton',
      TOOLBAR = 'toolbar',
      TOOLTIP = 'tooltip',
      TREE = 'tree',
      TREE_GRID = 'treeGrid',
      TREE_ITEM = 'treeItem',
      UNKNOWN = 'unknown',
      VIDEO = 'video',
      WEB_VIEW = 'webView',
      WINDOW = 'window',
    }

    export enum StateType {
      AUTOFILL_AVAILABLE = 'autofillAvailable',
      COLLAPSED = 'collapsed',
      DEFAULT = 'default',
      EDITABLE = 'editable',
      EXPANDED = 'expanded',
      FOCUSABLE = 'focusable',
      FOCUSED = 'focused',
      HORIZONTAL = 'horizontal',
      HOVERED = 'hovered',
      IGNORED = 'ignored',
      INVISIBLE = 'invisible',
      LINKED = 'linked',
      MULTILINE = 'multiline',
      MULTISELECTABLE = 'multiselectable',
      OFFSCREEN = 'offscreen',
      PROTECTED = 'protected',
      REQUIRED = 'required',
      RICHLY_EDITABLE = 'richlyEditable',
      VERTICAL = 'vertical',
      VISITED = 'visited',
    }

    export enum ActionType {
      ANNOTATE_PAGE_IMAGES = 'annotatePageImages',
      BLUR = 'blur',
      CLEAR_ACCESSIBILITY_FOCUS = 'clearAccessibilityFocus',
      COLLAPSE = 'collapse',
      CUSTOM_ACTION = 'customAction',
      DECREMENT = 'decrement',
      DO_DEFAULT = 'doDefault',
      EXPAND = 'expand',
      FOCUS = 'focus',
      GET_IMAGE_DATA = 'getImageData',
      GET_TEXT_LOCATION = 'getTextLocation',
      HIDE_TOOLTIP = 'hideTooltip',
      HIT_TEST = 'hitTest',
      INCREMENT = 'increment',
      INTERNAL_INVALIDATE_TREE = 'internalInvalidateTree',
      LOAD_INLINE_TEXT_BOXES = 'loadInlineTextBoxes',
      LONG_CLICK = 'longClick',
      REPLACE_SELECTED_TEXT = 'replaceSelectedText',
      RESUME_MEDIA = 'resumeMedia',
      SCROLL_BACKWARD = 'scrollBackward',
      SCROLL_DOWN = 'scrollDown',
      SCROLL_FORWARD = 'scrollForward',
      SCROLL_LEFT = 'scrollLeft',
      SCROLL_RIGHT = 'scrollRight',
      SCROLL_UP = 'scrollUp',
      SCROLL_TO_MAKE_VISIBLE = 'scrollToMakeVisible',
      SCROLL_TO_POINT = 'scrollToPoint',
      SCROLL_TO_POSITION_AT_ROW_COLUMN = 'scrollToPositionAtRowColumn',
      SET_ACCESSIBILITY_FOCUS = 'setAccessibilityFocus',
      SET_SCROLL_OFFSET = 'setSCrollOffset',
      SET_SELECTION = 'setSelection',
      SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT =
          'setSequentialFocusNavigationStartingPoint',
      SET_VALUE = 'setValue',
      SHOW_CONTEXT_MENU = 'showContextMenu',
      SIGNAL_END_OF_TEST = 'signalEndOfTest',
      SHOW_TOOLTIP = 'showTooltip',
      START_DUCKING_MEDIA = 'startDuckingMedia',
      STOP_DUCKING_MEDIA = 'stopDuckingMedia',
      SUSPEND_MEDIA = 'suspendMedia',
    }

    export enum TreeChangeType {
      NODE_CHANGED = 'nodeChanged',
      NODE_CREATED = 'nodeCreated',
      NODE_REMOVED = 'nodeRemoved',
      SUBTREE_CREATED = 'subtreeCreated',
      SUBTREE_UPDATE_END = 'subtreeUpdateEnd',
      TEXT_CHANGED = 'textChanged',
    }

    export enum NameFromType {
      ATTRIBUTE = 'attribute',
      ATTRIBUTE_EXPLICITLY_EMPTY = 'attributeExplicitlyEmpty',
      CAPTION = 'caption',
      CONTENTS = 'contents',
      PLACEHOLDER = 'placeholder',
      RELATED_ELEMENT = 'relatedElement',
      TITLE = 'title',
      VALUE = 'value',
    }

    export enum DescriptionFromType {
      ARIA_DESCRIPTION = 'ariaDescription',
      ATTRIBUTE_EXPLICITLY_EMPTY = 'attributeExplicitlyEmpty',
      BUTTON_LABEL = 'buttonLabel',
      POPOVER_ATTRIBUTE = 'popoverAttribute',
      RELATED_ELEMENT = 'relatedElement',
      RUBY_ANNOTATION = 'rubyAnnotation',
      SUMMARY = 'summary',
      SVG_DESC_ELEMENT = 'svgDescElement',
      TABLE_CAPTION = 'tableCaption',
      TITLE = 'title',
    }

    export enum Restriction {
      DISABLED = 'disabled',
      READ_ONLY = 'readOnly',
    }

    export enum HasPopup {
      DIALOG = 'dialog',
      FALSE = 'false',
      GRID = 'grid',
      LISTBOX = 'listbox',
      MENU = 'menu',
      TREE = 'tree',
      TRUE = 'true',
    }

    export enum AriaCurrentState {
      DATE = 'date',
      FALSE = 'false',
      LOCATION = 'location',
      PAGE = 'page',
      STEP = 'step',
      TIME = 'time',
      TRUE = 'true',
    }

    export enum InvalidState {
      FALSE = 'false',
      TRUE = 'true',
    }

    export enum DefaultActionVerb {
      ACTIVATE = 'activate',
      CHECK = 'check',
      CLICK = 'click',
      CLICK_ANCESTOR = 'clickAncestor',
      JUMP = 'jump',
      OPEN = 'open',
      PRESS = 'press',
      SELECT = 'select',
      UNCHECK = 'uncheck',
    }

    export enum MarkerType {
      ACTIVE_SUGGESTION = 'activeSuggestion',
      GRAMMAR = 'grammar',
      HIGHLIGHT = 'highlight',
      SPELLING = 'spelling',
      SUGGESTION = 'suggestion',
      TEXT_MATCH = 'textMatch',
    }

    export enum IntentCommandType {
      CLEAR_SELECTION = 'clearSelection',
      DELETE = 'delete',
      DICTATE = 'dictate',
      EXTEND_SELECTION = 'extendSelection',
      FORMAT = 'format',
      HISTORY = 'history',
      INSERT = 'insert',
      MARKER = 'marker',
      MOVE_SELECTION = 'moveSelection',
      SET_SELECTION = 'setSelection',
    }

    export enum IntentInputEventType {
      DELETE_BY_CUT = 'deleteByCut',
      DELETE_BY_DRAG = 'deleteByDrag',
      DELETE_CONTENT_BACKWARD = 'deleteContentBackward',
      DELETE_CONTENT_FORWARD = 'deleteContentForward',
      DELETE_HARD_LINE_BACKWARD = 'deleteHardLineBackward',
      DELETE_HARD_LINE_FORWARD = 'deleteHardLineForward',
      DELETE_SOFT_LINE_BACKWARD = 'deleteSoftLineBackward',
      DELETE_SOFT_LINE_FORWARD = 'deleteSoftLineForward',
      DELETE_WORD_BACKWARD = 'deleteWordBackward',
      DELETE_WORD_FORWARD = 'deleteWordForward',
      FORMAT_BOLD = 'formatBold',
      FORMAT_INDENT = 'formatIndent',
      FORMAT_ITALIC = 'formatItalic',
      FORMAT_JUSTIFY_CENTER = 'formatJustifyCenter',
      FORMAT_JUSTIFY_FULL = 'formatJustifyFull',
      FORMAT_JUSTIFY_LEFT = 'formatJustifyLeft',
      FORMAT_JUSTIFY_RIGHT = 'formatJustifyRight',
      FORMAT_OUTDENT = 'formatOutdent',
      FORMAT_REMOVE = 'formatRemove',
      FORMAT_SET_BLOCK_TEXT_DIRECTION = 'formatSetBlockTextDirection',
      FORMAT_STRIKE_THROUGH = 'formatStrikeThrough',
      FORMAT_SUBSCRIPT = 'formatSubscript',
      FORMAT_SUPERSCRIPT = 'formatSuperscript',
      FORMAT_UNDERLINE = 'formatUnderline',
      HISTORY_REDO = 'historyRedo',
      HISTORY_UNDO = 'historyUndo',
      INSERT_COMPOSITION_TEXT = 'insertCompositionText',
      INSERT_FROM_DROP = 'insertFromDrop',
      INSERT_FROM_PASTE = 'insertFromPaste',
      INSERT_FROM_YANK = 'insertFromYank',
      INSERT_HORIZONTAL_RULE = 'insertHorizontalRule',
      INSERT_LINE_BREAK = 'insertLineBreak',
      INSERT_ORDERED_LIST = 'insertOrderedList',
      INSERT_PARAGRAPH = 'insertParagraph',
      INSERT_REPLACEMENT_TEXT = 'insertReplacementText',
      INSERT_TEXT = 'insertText',
      INSERT_TRANSPOSE = 'insertTranspose',
      INSERT_UNORDERED_LIST = 'insertUnorderedList',
    }

    enum IntentTextBoundaryType {
      CHARACTER = 'character',
      FORMAT_END = 'formatEnd',
      FORMAT_START = 'formatStart',
      FORMAT_START_OR_END = 'formatStartOrEnd',
      LINE_END = 'lineEnd',
      LINE_START = 'lineStart',
      LINE_START_OR_END = 'lineStartOrEnd',
      OBJECT = 'object',
      PAGE_END = 'pageEnd',
      PAGE_START = 'pageStart',
      PAGE_START_OR_END = 'pageStartOrEnd',
      PARAGRAPH_END = 'paragraphEnd',
      PARAGRAPH_START = 'paragraphStart',
      PARAGRAPH_START_OR_END = 'paragraphStartOrEnd',
      PARAGRAPH_START_SKIPPING_EMPTY_PARAGRAPHS =
          'paragraphStartSkippingEmptyParagraphs',
      SENTENCE_END = 'sentenceEnd',
      SENTENCE_START = 'sentenceStart',
      SENTENCE_START_OR_END = 'sentenceStartOrEnd',
      WEB_PAGE = 'webPage',
      WORD_END = 'wordEnd',
      WORD_START = 'wordStart',
      WORD_START_OR_END = 'wordStartOrEnd',
    }

    export enum IntentMoveDirectionType {
      BACKWARD = 'backward',
      FORWARD = 'forward',
    }

    export enum SortDirectionType {
      ASCENDING = 'ascending',
      DESCENDING = 'descending',
      OTHER = 'other',
      UNSORTED = 'unsorted',
    }

    export enum PositionType {
      NULL = 'null',
      TEXT = 'text',
      TREE = 'tree',
    }

    export enum TreeChangeObserverFilter {
      ALL_TREE_CHANGES = 'allTreeChanges',
      LIVE_REGION_TREE_CHANGES = 'liveRegionTreeChanges',
      NO_TREE_CHANGES = 'noTreeChanges',
      TEXT_MARKER_CHANGES = 'textMarkerChanges',
    }

    export interface Rect {
      left: number;
      height: number;
      top: number;
      width: number;
    }

    export interface FindParams {
      attributes?: object;
      role?: RoleType;
      state?: object;
    }

    export interface SetDocumentSelectionParams {
      anchorObject: AutomationNode;
      anchorOffset: number;
      focusObject: AutomationNode;
      focusOffset: number;
    }

    export interface AutomationIntent {
      command: IntentCommandType;
      moveDirection: IntentMoveDirectionType;
      textBoundary: IntentTextBoundaryType;
    }

    export interface AutomationEvent {
      eventFrom: string;
      intents: AutomationIntent[];
      mouseX?: number;
      mouseY?: number;
      target: AutomationNode;
      type: EventType;

      stopPropagation(): void;
    }

    export interface TreeChange {
      target: AutomationNode;
      type: TreeChangeType;
    }

    export interface CustomAction {
      description: string;
      id: number;
    }

    export interface Marker {
      endOffset: number;
      flags: {[key in MarkerType]: boolean;};
      startOffset: number;
    }

    export interface AutomationPosition {
      affinity: string;
      childIndex?: number;
      node?: AutomationNode;
      textOffset?: number;

      asLeafTextPosition(): void;
      asTextPosition(): void;
      asTreePosition(): void;
      atEndOfAnchor(): boolean;
      atEndOfDocument(): boolean;
      atEndOfFormat(): boolean;
      atEndOfLine(): boolean;
      atEndOfPage(): boolean;
      atEndOfParagraph(): boolean;
      atEndOfWord(): boolean;
      atStartOfAnchor(): boolean;
      atStartOfDocument(): boolean;
      atStartOfFormat(): boolean;
      atStartOfLine(): boolean;
      atStartOfPage(): boolean;
      atStartOfParagraph(): boolean;
      atStartOfWord(): boolean;
      getText(): string;
      isInLineBreak(): boolean;
      isInTextObject(): boolean;
      isInWhiteSpace(): boolean;
      isLeafTextPosition(): boolean;
      isNullPosition(): boolean;
      isTextPosition(): boolean;
      isTreePosition(): boolean;
      isValid(): boolean;
      moveToNextAnchorPosition(): void;
      moveToNextCharacterPosition(): void;
      moveToNextFormatEndPosition(): void;
      moveToNextFormatStartPosition(): void;
      moveToNextLeafTextPosition(): void;
      moveToNextLeafTreePosition(): void;
      moveToNextLineEndPosition(): void;
      moveToNextLineStartPosition(): void;
      moveToNextPageEndPosition(): void;
      moveToNextPageStartPosition(): void;
      moveToNextParagraphEndPosition(): void;
      moveToNextParagraphStartPosition(): void;
      moveToNextWordEndPosition(): void;
      moveToNextWordStartPosition(): void;
      moveToParentPosition(): void;
      moveToPositionAtEndOfAnchor(): void;
      moveToPositionAtEndOfDocument(): void;
      moveToPositionAtStartOfAnchor(): void;
      moveToPositionAtStartOfDocument(): void;
      moveToPreviousAnchorPosition(): void;
      moveToPreviousCharacterPosition(): void;
      moveToPreviousFormatEndPosition(): void;
      moveToPreviousFormatStartPosition(): void;
      moveToPreviousLeafTextPosition(): void;
      moveToPreviousLeafTreePosition(): void;
      moveToPreviousLineEndPosition(): void;
      moveToPreviousLineStartPosition(): void;
      moveToPreviousPageEndPosition(): void;
      moveToPreviousPageStartPosition(): void;
      moveToPreviousParagraphEndPosition(): void;
      moveToPreviousParagraphStartPosition(): void;
      moveToPreviousWordStartPosition(): void;
      moveToPreviousWordEndPosition(): void;
    }

    export interface AutomationNode {
      accessKey?: string;
      activeDescendant?: AutomationNode;
      activeDescendantFor?: AutomationNode[];
      anchorAffinity?: string;
      anchorObject?: AutomationNode;
      anchorOffset?: number;
      appId?: string;
      ariaColumnCount?: number;
      ariaCurrentState?: AriaCurrentState;
      ariaInvalidValue?: string;
      ariaRowCount?: number;
      autoComplete?: string;
      backgroundColor?: number;
      bold: boolean;
      busy?: boolean;
      checked?: string;
      checkedStateDescription?: string;
      children: AutomationNode[];
      className?: string;
      clickable: boolean;
      color?: number;
      colorValue?: number;
      containerLiveAtomic?: boolean;
      containerLiveBusy?: boolean;
      containerLiveRelevant?: string;
      containerLiveStatus?: string;
      controlledBy?: AutomationNode[];
      controls?: AutomationNode[];
      customActions?: CustomAction[];
      defaultActionVerb?: DefaultActionVerb;
      describedBy?: AutomationNode[];
      descriptionFor?: AutomationNode[];
      details?: AutomationNode[];
      detailsFor?: AutomationNode[];
      detectedLanguage?: string;
      display?: string;
      docLoaded?: boolean;
      docLoadingProgress?: number;
      docUrl?: string;
      docTitle?: string;
      doDefaultLabel?: string;
      errorMessage?: AutomationNode;
      errorMessageFor?: AutomationNode[];
      firstChild?: AutomationNode;
      flowFrom?: AutomationNode[];
      flowTo?: AutomationNode[];
      focusAffinity?: string;
      focusObject?: AutomationNode;
      focusOffset?: number;
      fontFamily?: string;
      fontSize?: number;
      hasPopup?: HasPopup;
      hierarchicalLevel?: number;
      htmlAttributes?: {
        [key: string]: string,
      };
      htmlTag?: string;
      imageAnnotation?: string;
      imageDataUrl?: string;
      indexInParent?: number;
      innerHtml?: string;
      inPageLinkTarget?: AutomationNode;
      inputType?: string;
      invalidState?: InvalidState;
      isButton: boolean;
      isCheckBox: boolean;
      isComboBox: boolean;
      isImage: boolean;
      isRootNode: boolean;
      isSelectionBackward?: boolean;
      italic: boolean;
      labelFor?: AutomationNode[];
      labelledBy?: AutomationNode[];
      language?: string;
      lastChild?: AutomationNode;
      lineThrough: boolean;
      liveAtomic?: boolean;
      liveRelevant?: string;
      liveStatus?: string;
      location?: Rect;
      longClickLabel?: string;
      markers?: Marker[];
      maxValueForRange?: number;
      minValueForRange?: number;
      modal?: boolean;
      name?: string;
      nameFrom?: NameFromType;
      nextFocus?: AutomationNode;
      nextOnLine?: AutomationNode;
      nextSibling?: AutomationNode;
      nonAtomicTextFieldRoot: boolean;
      nonInlineTextWordStarts?: number[];
      nonInlineTextWordEnds?: number[];
      notUserSelectableStyle?: boolean;
      parent?: AutomationNode;
      placeholder?: string;
      posInSet?: number;
      previousFocus?: AutomationNode;
      previousOnLine?: AutomationNode;
      previousSibling?: AutomationNode;
      restriction?: Restriction;
      role?: RoleType;
      roleDescription?: string;
      root?: AutomationNode;
      scrollable?: boolean;
      scrollX?: number;
      scrollXMin?: number;
      scrollXMax?: number;
      scrollY?: number;
      scrollYMin?: number;
      scrollYMax?: number;
      selected?: boolean;
      selectionEndAffinity?: string;
      selectionEndObject?: AutomationNode;
      selectionEndOffset?: number;
      selectionStartAffinity?: string;
      selectionStartObject?: AutomationNode;
      selectionStartOffset?: number;
      sentenceEnds?: number[];
      sentenceStarts?: number[];
      setSize?: number;
      sortDirection: SortDirectionType;
      standardActions?: ActionType[];
      state?: {[key in StateType]: boolean;};
      subscript: boolean;
      superscript: boolean;
      tableCellAriaColumnIndex?: number;
      tableCellAriaRowIndex?: number;
      tableCellColumnHeaders?: AutomationNode[];
      tableCellColumnIndex?: number;
      tableCellRowHeaders?: AutomationNode[];
      tableCellRowIndex?: number;
      tableCellRowSpan?: number;
      tableColumnCount?: number;
      tableColumnHeader?: AutomationNode;
      tableColumnIndex?: number;
      tableRowCount?: number;
      tableRowHeader?: AutomationNode;
      tableRowIndex?: number;
      textSelEnd?: number;
      textSelStart?: number;
      tooltip?: string;
      unclippedLocation?: Rect;
      underline: boolean;
      url?: string;
      value?: string;
      valueForRange?: number;
      wordStarts?: number[];
      wordEnds?: number[];


      addEventListener(
          type: EventType, listener: AutomationListener,
          capture: boolean): void;
      boundsForRange(
          startIndex: number, endIndex: number,
          callback: BoundsForRangeCallback): void;
      createPosition(type: PositionType, offset: number, isUpstream?: boolean):
          AutomationPosition;
      doDefault(): void;
      find(params: FindParams): AutomationNode|undefined;
      findAll(params: FindParams): AutomationNode[];
      focus(): void;
      getImageData(maxWidth: number, maxHeight: number): void;
      getNextTextMatch(searchStr: string, backward: boolean): AutomationNode;
      hitTest(x: number, y: number, eventToFire: EventType): void;
      hitTestWithReply(
          x: number, y: number, callback: PerformActionCallbackWithNode): void;
      longClick(): void;
      makeVisible(): void;
      matches(params: FindParams): boolean;
      performCustomAction(id: number): void;
      performStandardAction(type: ActionType): void;
      removeEventListener(
          type: EventType, listener: AutomationListener,
          capture: boolean): void;
      replaceSelectedText(value: string): void;
      resumeMedia(): void;
      scrollBackward(callback?: PerformActionCallback): void;
      scrollDown(callback?: PerformActionCallback): void;
      scrollForward(callback?: PerformActionCallback): void;
      scrollLeft(callback?: PerformActionCallback): void;
      scrollRight(callback?: PerformActionCallback): void;
      scrollToPoint(x: number, y: number): void;
      scrollUp(callback?: PerformActionCallback): void;
      setAccessibilityFocus(): void;
      setScrollOffset(x: number, y: number): void;
      setSelection(startIndex: number, endIndex: number): void;
      setSequentialFocusNavigationStartingPoint(): void;
      setValue(value: string): void;
      showContextMenu(): void;
      startDuckingMedia(): void;
      stopDuckingMedia(): void;
      suspendMedia(): void;
      unclippedBoundsForRange(
          startIndex: number, endIndex: number,
          callback: BoundsForRangeCallback): void;
    }

    type AutomationListener = (event: AutomationEvent) => void;
    type BoundsForRangeCallback = (bounds: Rect) => void;
    type FocusCallback = (focusedNode: AutomationNode) => void;
    type PerformActionCallback = (result: boolean) => void;
    type PerformActionCallbackWithNode = (node: AutomationNode) => void;
    type RootCallback = (rootNode: AutomationNode) => void;
    type TreeChangeObserver = (change: TreeChange) => void;

    export function addTreeChangeObserver(
        filter: TreeChangeObserverFilter, observer: TreeChangeObserver): void;
    export function getAccessibilityFocus(callback: FocusCallback): void;
    export function getDesktop(callback: RootCallback): void;
    export function getFocus(callback: FocusCallback): void;
    export function removeTreeChangeObserver(observer: TreeChangeObserver):
        void;
    export function setDocumentSelection(params: SetDocumentSelectionParams):
        void;
  }
}
