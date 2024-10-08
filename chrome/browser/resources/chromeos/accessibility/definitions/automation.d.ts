// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.automation API
 * Generated from: extensions/common/api/automation.idl
 * run `tools/json_schema_compiler/compiler.py
 * extensions/common/api/automation.idl -g ts_definitions` to regenerate.
 */



declare namespace chrome {
  export namespace automation {

    export enum EventType {
      ACCESS_KEY_CHANGED = 'accessKeyChanged',
      ACTIVE_DESCENDANT_CHANGED = 'activeDescendantChanged',
      ALERT = 'alert',
      ARIA_ATTRIBUTE_CHANGED_DEPRECATED = 'ariaAttributeChangedDeprecated',
      ARIA_CURRENT_CHANGED = 'ariaCurrentChanged',
      ARIA_NOTIFICATIONS_POSTED = 'ariaNotificationsPosted',
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
      MENU_LIST_VALUE_CHANGED_DEPRECATED = 'menuListValueChangedDeprecated',
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
      ORIENTATION_CHANGED = 'orientationChanged',
      PARENT_CHANGED = 'parentChanged',
      PLACEHOLDER_CHANGED = 'placeholderChanged',
      POSITION_IN_SET_CHANGED = 'positionInSetChanged',
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
      SCROLLED_TO_ANCHOR = 'scrolledToAnchor',
      SELECTED_CHANGED = 'selectedChanged',
      SELECTED_CHILDREN_CHANGED = 'selectedChildrenChanged',
      SELECTED_VALUE_CHANGED = 'selectedValueChanged',
      SELECTION = 'selection',
      SELECTION_ADD = 'selectionAdd',
      SELECTION_REMOVE = 'selectionRemove',
      SET_SIZE_CHANGED = 'setSizeChanged',
      SHOW = 'show',
      SORT_CHANGED = 'sortChanged',
      STATE_CHANGED = 'stateChanged',
      SUBTREE_CREATED = 'subtreeCreated',
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
      WINDOW_VISIBILITY_CHANGED = 'windowVisibilityChanged',
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
      DESCRIPTION_LIST_DETAIL_DEPRECATED = 'descriptionListDetailDeprecated',
      DESCRIPTION_LIST_TERM_DEPRECATED = 'descriptionListTermDeprecated',
      DESKTOP = 'desktop',
      DETAILS = 'details',
      DIALOG = 'dialog',
      DIRECTORY_DEPRECATED = 'directoryDeprecated',
      DISCLOSURE_TRIANGLE = 'disclosureTriangle',
      DISCLOSURE_TRIANGLE_GROUPED = 'disclosureTriangleGrouped',
      DOC_ABSTRACT = 'docAbstract',
      DOC_ACKNOWLEDGMENTS = 'docAcknowledgments',
      DOC_AFTERWORD = 'docAfterword',
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
      DOC_QNA = 'docQna',
      DOC_SUBTITLE = 'docSubtitle',
      DOC_TIP = 'docTip',
      DOC_TOC = 'docToc',
      DOCUMENT = 'document',
      EMBEDDED_OBJECT = 'embeddedObject',
      EMPHASIS = 'emphasis',
      FEED = 'feed',
      FIGCAPTION = 'figcaption',
      FIGURE = 'figure',
      FOOTER = 'footer',
      FORM = 'form',
      GENERIC_CONTAINER = 'genericContainer',
      GRAPHICS_DOCUMENT = 'graphicsDocument',
      GRAPHICS_OBJECT = 'graphicsObject',
      GRAPHICS_SYMBOL = 'graphicsSymbol',
      GRID = 'grid',
      GRID_CELL = 'gridCell',
      GROUP = 'group',
      HEADER = 'header',
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
      MATH_MLFRACTION = 'mathMLFraction',
      MATH_MLIDENTIFIER = 'mathMLIdentifier',
      MATH_MLMATH = 'mathMLMath',
      MATH_MLMULTISCRIPTS = 'mathMLMultiscripts',
      MATH_MLNONE_SCRIPT = 'mathMLNoneScript',
      MATH_MLNUMBER = 'mathMLNumber',
      MATH_MLOPERATOR = 'mathMLOperator',
      MATH_MLOVER = 'mathMLOver',
      MATH_MLPRESCRIPT_DELIMITER = 'mathMLPrescriptDelimiter',
      MATH_MLROOT = 'mathMLRoot',
      MATH_MLROW = 'mathMLRow',
      MATH_MLSQUARE_ROOT = 'mathMLSquareRoot',
      MATH_MLSTRING_LITERAL = 'mathMLStringLiteral',
      MATH_MLSUB = 'mathMLSub',
      MATH_MLSUB_SUP = 'mathMLSubSup',
      MATH_MLSUP = 'mathMLSup',
      MATH_MLTABLE = 'mathMLTable',
      MATH_MLTABLE_CELL = 'mathMLTableCell',
      MATH_MLTABLE_ROW = 'mathMLTableRow',
      MATH_MLTEXT = 'mathMLText',
      MATH_MLUNDER = 'mathMLUnder',
      MATH_MLUNDER_OVER = 'mathMLUnderOver',
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
      PORTAL_DEPRECATED = 'portalDeprecated',
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
      SECTION_FOOTER = 'sectionFooter',
      SECTION_HEADER = 'sectionHeader',
      SECTION_WITHOUT_NAME = 'sectionWithoutName',
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
      SET_SCROLL_OFFSET = 'setScrollOffset',
      SET_SELECTION = 'setSelection',
      SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT =
          'setSequentialFocusNavigationStartingPoint',
      SET_VALUE = 'setValue',
      SHOW_CONTEXT_MENU = 'showContextMenu',
      SIGNAL_END_OF_TEST = 'signalEndOfTest',
      SHOW_TOOLTIP = 'showTooltip',
      STITCH_CHILD_TREE = 'stitchChildTree',
      START_DUCKING_MEDIA = 'startDuckingMedia',
      STOP_DUCKING_MEDIA = 'stopDuckingMedia',
      SUSPEND_MEDIA = 'suspendMedia',
    }

    export enum TreeChangeType {
      NODE_CREATED = 'nodeCreated',
      SUBTREE_CREATED = 'subtreeCreated',
      NODE_CHANGED = 'nodeChanged',
      TEXT_CHANGED = 'textChanged',
      NODE_REMOVED = 'nodeRemoved',
      SUBTREE_UPDATE_END = 'subtreeUpdateEnd',
    }

    export enum NameFromType {
      ATTRIBUTE = 'attribute',
      ATTRIBUTE_EXPLICITLY_EMPTY = 'attributeExplicitlyEmpty',
      CAPTION = 'caption',
      CONTENTS = 'contents',
      CSSALTTEXT = 'cssAltText',
      PLACEHOLDER = 'placeholder',
      POPOVER_ATTRIBUTE = 'popoverAttribute',
      PROHIBITED = 'prohibited',
      RELATED_ELEMENT = 'relatedElement',
      TITLE = 'title',
      VALUE = 'value',
    }

    export enum DescriptionFromType {
      ARIA_DESCRIPTION = 'ariaDescription',
      ATTRIBUTE_EXPLICITLY_EMPTY = 'attributeExplicitlyEmpty',
      BUTTON_LABEL = 'buttonLabel',
      POPOVER_ATTRIBUTE = 'popoverAttribute',
      PROHIBITED_NAME_REPAIR = 'prohibitedNameRepair',
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
      FALSE = 'false',
      TRUE = 'true',
      MENU = 'menu',
      LISTBOX = 'listbox',
      TREE = 'tree',
      GRID = 'grid',
      DIALOG = 'dialog',
    }

    export enum AriaCurrentState {
      FALSE = 'false',
      TRUE = 'true',
      PAGE = 'page',
      STEP = 'step',
      LOCATION = 'location',
      DATE = 'date',
      TIME = 'time',
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
      SPELLING = 'spelling',
      GRAMMAR = 'grammar',
      TEXT_MATCH = 'textMatch',
      ACTIVE_SUGGESTION = 'activeSuggestion',
      SUGGESTION = 'suggestion',
      HIGHLIGHT = 'highlight',
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
      INSERT_TEXT = 'insertText',
      INSERT_LINE_BREAK = 'insertLineBreak',
      INSERT_PARAGRAPH = 'insertParagraph',
      INSERT_ORDERED_LIST = 'insertOrderedList',
      INSERT_UNORDERED_LIST = 'insertUnorderedList',
      INSERT_HORIZONTAL_RULE = 'insertHorizontalRule',
      INSERT_FROM_PASTE = 'insertFromPaste',
      INSERT_FROM_DROP = 'insertFromDrop',
      INSERT_FROM_YANK = 'insertFromYank',
      INSERT_TRANSPOSE = 'insertTranspose',
      INSERT_REPLACEMENT_TEXT = 'insertReplacementText',
      INSERT_COMPOSITION_TEXT = 'insertCompositionText',
      INSERT_LINK = 'insertLink',
      DELETE_WORD_BACKWARD = 'deleteWordBackward',
      DELETE_WORD_FORWARD = 'deleteWordForward',
      DELETE_SOFT_LINE_BACKWARD = 'deleteSoftLineBackward',
      DELETE_SOFT_LINE_FORWARD = 'deleteSoftLineForward',
      DELETE_HARD_LINE_BACKWARD = 'deleteHardLineBackward',
      DELETE_HARD_LINE_FORWARD = 'deleteHardLineForward',
      DELETE_CONTENT_BACKWARD = 'deleteContentBackward',
      DELETE_CONTENT_FORWARD = 'deleteContentForward',
      DELETE_BY_CUT = 'deleteByCut',
      DELETE_BY_DRAG = 'deleteByDrag',
      HISTORY_UNDO = 'historyUndo',
      HISTORY_REDO = 'historyRedo',
      FORMAT_BOLD = 'formatBold',
      FORMAT_ITALIC = 'formatItalic',
      FORMAT_UNDERLINE = 'formatUnderline',
      FORMAT_STRIKE_THROUGH = 'formatStrikeThrough',
      FORMAT_SUPERSCRIPT = 'formatSuperscript',
      FORMAT_SUBSCRIPT = 'formatSubscript',
      FORMAT_JUSTIFY_CENTER = 'formatJustifyCenter',
      FORMAT_JUSTIFY_FULL = 'formatJustifyFull',
      FORMAT_JUSTIFY_RIGHT = 'formatJustifyRight',
      FORMAT_JUSTIFY_LEFT = 'formatJustifyLeft',
      FORMAT_INDENT = 'formatIndent',
      FORMAT_OUTDENT = 'formatOutdent',
      FORMAT_REMOVE = 'formatRemove',
      FORMAT_SET_BLOCK_TEXT_DIRECTION = 'formatSetBlockTextDirection',
    }

    export enum IntentTextBoundaryType {
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
      PARAGRAPH_START_SKIPPING_EMPTY_PARAGRAPHS =
          'paragraphStartSkippingEmptyParagraphs',
      PARAGRAPH_START_OR_END = 'paragraphStartOrEnd',
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
      UNSORTED = 'unsorted',
      ASCENDING = 'ascending',
      DESCENDING = 'descending',
      OTHER = 'other',
    }

    export enum PositionType {
      NULL = 'null',
      TEXT = 'text',
      TREE = 'tree',
    }

    export interface Rect {
      left: number;
      top: number;
      width: number;
      height: number;
    }

    export interface FindParams {
      role?: RoleType;
      state?: {
        [key: string]: any,
      };
      attributes?: {
        [key: string]: any,
      };
    }

    export interface SetDocumentSelectionParams {
      anchorObject: AutomationNode;
      anchorOffset: number;
      focusObject: AutomationNode;
      focusOffset: number;
    }

    export interface AutomationIntent {
      command: IntentCommandType;
      textBoundary: IntentTextBoundaryType;
      moveDirection: IntentMoveDirectionType;
    }

    export interface AutomationEvent {
      target: AutomationNode;
      type: EventType;
      eventFrom: string;
      mouseX: number;
      mouseY: number;
      intents: AutomationIntent[];
      stopPropagation: () => void;
    }

    export interface TreeChange {
      target: AutomationNode;
      type: TreeChangeType;
    }

    export enum TreeChangeObserverFilter {
      NO_TREE_CHANGES = 'noTreeChanges',
      LIVE_REGION_TREE_CHANGES = 'liveRegionTreeChanges',
      TEXT_MARKER_CHANGES = 'textMarkerChanges',
      ALL_TREE_CHANGES = 'allTreeChanges',
    }

    export interface CustomAction {
      id: number;
      description: string;
    }

    export interface Marker {
      startOffset: number;
      endOffset: number;
      flags: {
        [key: string]: any,
      };
    }

    export interface AutomationPosition {
      node?: AutomationNode;
      childIndex: number;
      textOffset: number;
      affinity: string;
      isNullPosition: () => boolean;
      isTreePosition: () => boolean;
      isTextPosition: () => boolean;
      isLeafTextPosition: () => boolean;
      atStartOfAnchor: () => boolean;
      atEndOfAnchor: () => boolean;
      atStartOfWord: () => boolean;
      atEndOfWord: () => boolean;
      atStartOfLine: () => boolean;
      atEndOfLine: () => boolean;
      atStartOfParagraph: () => boolean;
      atEndOfParagraph: () => boolean;
      atStartOfPage: () => boolean;
      atEndOfPage: () => boolean;
      atStartOfFormat: () => boolean;
      atEndOfFormat: () => boolean;
      atStartOfDocument: () => boolean;
      atEndOfDocument: () => boolean;
      asTreePosition: () => void;
      asTextPosition: () => void;
      asLeafTextPosition: () => void;
      moveToPositionAtStartOfAnchor: () => void;
      moveToPositionAtEndOfAnchor: () => void;
      moveToPositionAtStartOfDocument: () => void;
      moveToPositionAtEndOfDocument: () => void;
      moveToParentPosition: () => void;
      moveToNextLeafTreePosition: () => void;
      moveToPreviousLeafTreePosition: () => void;
      moveToNextLeafTextPosition: () => void;
      moveToPreviousLeafTextPosition: () => void;
      moveToNextCharacterPosition: () => void;
      moveToPreviousCharacterPosition: () => void;
      moveToNextWordStartPosition: () => void;
      moveToPreviousWordStartPosition: () => void;
      moveToNextWordEndPosition: () => void;
      moveToPreviousWordEndPosition: () => void;
      moveToNextLineStartPosition: () => void;
      moveToPreviousLineStartPosition: () => void;
      moveToNextLineEndPosition: () => void;
      moveToPreviousLineEndPosition: () => void;
      moveToNextFormatStartPosition: () => void;
      moveToPreviousFormatStartPosition: () => void;
      moveToNextFormatEndPosition: () => void;
      moveToPreviousFormatEndPosition: () => void;
      moveToNextParagraphStartPosition: () => void;
      moveToPreviousParagraphStartPosition: () => void;
      moveToNextParagraphEndPosition: () => void;
      moveToPreviousParagraphEndPosition: () => void;
      moveToNextPageStartPosition: () => void;
      moveToPreviousPageStartPosition: () => void;
      moveToNextPageEndPosition: () => void;
      moveToPreviousPageEndPosition: () => void;
      moveToNextAnchorPosition: () => void;
      moveToPreviousAnchorPosition: () => void;
      maxTextOffset: () => number;
      isInLineBreak: () => boolean;
      isInTextObject: () => boolean;
      isInWhiteSpace: () => boolean;
      isValid: () => boolean;
      getText: () => string;
    }

    export interface AutomationNode {
      root?: AutomationNode;
      isRootNode: boolean;
      role?: RoleType;
      state?: {
        [key: string]: any,
      };
      location: Rect;
      boundsForRange:
          (startIndex: number, endIndex: number,
           callback: (bounds: Rect) => void) => void;
      unclippedBoundsForRange:
          (startIndex: number, endIndex: number,
           callback: (bounds: Rect) => void) => void;
      unclippedLocation?: Rect;
      description?: string;
      checkedStateDescription?: string;
      placeholder?: string;
      roleDescription?: string;
      name?: string;
      doDefaultLabel?: string;
      longClickLabel?: string;
      tooltip?: string;
      nameFrom?: NameFromType;
      imageAnnotation?: string;
      value?: string;
      htmlId?: string;
      htmlTag?: string;
      hierarchicalLevel?: number;
      caretBounds?: Rect;
      wordStarts?: number[];
      wordEnds?: number[];
      sentenceStarts?: number[];
      sentenceEnds?: number[];
      nonInlineTextWordStarts?: number[];
      nonInlineTextWordEnds?: number[];
      controls?: AutomationNode[];
      describedBy?: AutomationNode[];
      flowTo?: AutomationNode[];
      labelledBy?: AutomationNode[];
      activeDescendant?: AutomationNode;
      activeDescendantFor?: AutomationNode[];
      inPageLinkTarget?: AutomationNode;
      details?: AutomationNode[];
      errorMessages?: AutomationNode[];
      detailsFor?: AutomationNode[];
      errorMessageFor?: AutomationNode[];
      controlledBy?: AutomationNode[];
      descriptionFor?: AutomationNode[];
      flowFrom?: AutomationNode[];
      labelFor?: AutomationNode[];
      tableCellColumnHeaders?: AutomationNode[];
      tableCellRowHeaders?: AutomationNode[];
      standardActions?: ActionType[];
      customActions?: CustomAction[];
      defaultActionVerb?: DefaultActionVerb;
      url?: string;
      docUrl?: string;
      docTitle?: string;
      docLoaded?: boolean;
      docLoadingProgress?: number;
      scrollX?: number;
      scrollXMin?: number;
      scrollXMax?: number;
      scrollY?: number;
      scrollYMin?: number;
      scrollYMax?: number;
      scrollable?: boolean;
      textSelStart?: number;
      textSelEnd?: number;
      markers?: Marker[];
      isSelectionBackward?: boolean;
      anchorObject?: AutomationNode;
      anchorOffset?: number;
      anchorAffinity?: string;
      focusObject?: AutomationNode;
      focusOffset?: number;
      focusAffinity?: string;
      selectionStartObject?: AutomationNode;
      selectionStartOffset?: number;
      selectionStartAffinity?: string;
      selectionEndObject?: AutomationNode;
      selectionEndOffset?: number;
      selectionEndAffinity?: string;
      notUserSelectableStyle?: boolean;
      valueForRange?: number;
      minValueForRange?: number;
      maxValueForRange?: number;
      posInSet?: number;
      setSize?: number;
      tableRowCount?: number;
      ariaRowCount?: number;
      tableColumnCount?: number;
      ariaColumnCount?: number;
      tableCellColumnIndex?: number;
      tableCellAriaColumnIndex?: number;
      ariaCellColumnIndexText?: string;
      tableCellColumnSpan?: number;
      tableCellRowIndex?: number;
      tableCellAriaRowIndex?: number;
      ariaCellRowIndexText?: string;
      tableCellRowSpan?: number;
      tableColumnHeader?: AutomationNode;
      tableRowHeader?: AutomationNode;
      tableColumnIndex?: number;
      tableRowIndex?: number;
      liveStatus?: string;
      liveRelevant?: string;
      liveAtomic?: boolean;
      busy?: boolean;
      containerLiveStatus?: string;
      containerLiveRelevant?: string;
      containerLiveAtomic?: boolean;
      containerLiveBusy?: boolean;
      isButton: boolean;
      isCheckBox: boolean;
      isComboBox: boolean;
      isImage: boolean;
      hasHiddenOffscreenNodes: boolean;
      autoComplete?: string;
      className?: string;
      modal?: boolean;
      inputType?: string;
      accessKey?: string;
      ariaInvalidValue?: string;
      display?: string;
      imageDataUrl?: string;
      language?: string;
      detectedLanguage?: string;
      hasPopup?: HasPopup;
      restriction?: string;
      checked?: string;
      mathContent?: string;
      color?: number;
      backgroundColor?: number;
      colorValue?: number;
      subscript: boolean;
      superscript: boolean;
      bold: boolean;
      italic: boolean;
      underline: boolean;
      lineThrough: boolean;
      selected?: boolean;
      fontSize?: number;
      fontFamily: string;
      nonAtomicTextFieldRoot: boolean;
      ariaCurrentState?: AriaCurrentState;
      invalidState?: InvalidState;
      appId?: string;
      children: AutomationNode[];
      parent?: AutomationNode;
      firstChild?: AutomationNode;
      lastChild?: AutomationNode;
      previousSibling?: AutomationNode;
      nextSibling?: AutomationNode;
      previousOnLine?: AutomationNode;
      nextOnLine?: AutomationNode;
      previousFocus?: AutomationNode;
      nextFocus?: AutomationNode;
      previousWindowFocus?: AutomationNode;
      nextWindowFocus?: AutomationNode;
      indexInParent?: number;
      sortDirection: SortDirectionType;
      clickable: boolean;
      doDefault: () => void;
      focus: () => void;
      getImageData: (maxWidth: number, maxHeight: number) => void;
      hitTest: (x: number, y: number, eventToFire: EventType) => void;
      hitTestWithReply:
          (x: number, y: number,
           callback: (node: AutomationNode) => void) => void;
      makeVisible: () => void;
      performCustomAction: (customActionId: number) => void;
      performStandardAction: (actionType: ActionType) => void;
      replaceSelectedText: (value: string) => void;
      setAccessibilityFocus: () => void;
      setSelection: (startIndex: number, endIndex: number) => void;
      setSequentialFocusNavigationStartingPoint: () => void;
      setValue: (value: string) => void;
      showContextMenu: () => void;
      resumeMedia: () => void;
      startDuckingMedia: () => void;
      stopDuckingMedia: () => void;
      suspendMedia: () => void;
      longClick: () => void;
      scrollBackward: (callback?: (result: boolean) => void) => void;
      scrollForward: (callback?: (result: boolean) => void) => void;
      scrollUp: (callback?: (result: boolean) => void) => void;
      scrollDown: (callback?: (result: boolean) => void) => void;
      scrollLeft: (callback?: (result: boolean) => void) => void;
      scrollRight: (callback?: (result: boolean) => void) => void;
      scrollToPoint: (x: number, y: number) => void;
      setScrollOffset: (x: number, y: number) => void;
      addEventListener:
          (eventType: EventType, listener: (event: AutomationEvent) => void,
           capture: boolean) => void;
      removeEventListener:
          (eventType: EventType, listener: (event: AutomationEvent) => void,
           capture: boolean) => void;
      find: (params: FindParams) => AutomationNode;
      findAll: (params: FindParams) => AutomationNode[];
      matches: (params: FindParams) => boolean;
      getNextTextMatch:
          (searchStr: string, backward: boolean) => AutomationNode;
      createPosition:
          (type: PositionType, offset: number,
           isUpstream?: boolean) => AutomationPosition;

      [key: string]: any;
    }

    export function getDesktop(callback: (node: AutomationNode) => void): void;

    export function getFocus(callback: (node: AutomationNode) => void): void;

    export function getAccessibilityFocus(
        callback: (node: AutomationNode) => void): void;

    export function addTreeChangeObserver(
        filter: TreeChangeObserverFilter,
        observer: (treeChange: TreeChange) => void): void;

    export function removeTreeChangeObserver(
        observer: (treeChange: TreeChange) => void): void;

    export function setDocumentSelection(params: SetDocumentSelectionParams):
        void;
  }
}
